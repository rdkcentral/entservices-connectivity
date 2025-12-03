/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "MatterClusterDelegates.h"
#include <lib/support/logging/CHIPLogging.h>
#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/time.h>

using namespace chip;
using namespace chip::app::Clusters;

namespace WPEFramework
{
    namespace Plugin
    {
        // Define supported key codes
        constexpr KeypadInput::CECKeyCodeEnum MatterKeypadInputDelegate::sSupportedKeyCodes[];

        MatterKeypadInputDelegate::MatterKeypadInputDelegate()
        {
            ChipLogProgress(AppServer, "MatterKeypadInputDelegate created");
            if (!InitializeUinput())
            {
                ChipLogError(AppServer, "Failed to initialize uinput device");
            }
        }

        MatterKeypadInputDelegate::~MatterKeypadInputDelegate()
        {
            CleanupUinput();
        }

        bool MatterKeypadInputDelegate::InitializeUinput()
        {
            mUinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Failed to open /dev/uinput: %s", strerror(errno));
                return false;
            }

            // Enable key events
            ioctl(mUinputFd, UI_SET_EVBIT, EV_KEY);
            ioctl(mUinputFd, UI_SET_EVBIT, EV_SYN);

            // Enable all key codes we might use
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_UP);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_DOWN);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_LEFT);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_RIGHT);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_ENTER);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_ESC);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_HOME);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_SETUP);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_0);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_1);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_2);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_3);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_4);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_5);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_6);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_7);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_8);
            ioctl(mUinputFd, UI_SET_KEYBIT, KEY_9);

            // Setup device
            struct uinput_setup usetup;
            memset(&usetup, 0, sizeof(usetup));
            usetup.id.bustype = BUS_USB;
            usetup.id.vendor = 0xbeef;
            usetup.id.product = 0xfedc;
            usetup.id.version = 1;
            strncpy(usetup.name, "matter-key-injector", UINPUT_MAX_NAME_SIZE);

            if (ioctl(mUinputFd, UI_DEV_SETUP, &usetup) < 0)
            {
                ChipLogError(AppServer, "Failed to setup uinput device: %s", strerror(errno));
                close(mUinputFd);
                mUinputFd = -1;
                return false;
            }

            if (ioctl(mUinputFd, UI_DEV_CREATE) < 0)
            {
                ChipLogError(AppServer, "Failed to create uinput device: %s", strerror(errno));
                close(mUinputFd);
                mUinputFd = -1;
                return false;
            }

            // Small delay for device to be ready
            usleep(50000);

            ChipLogProgress(AppServer, "Uinput device initialized successfully");
            return true;
        }

        void MatterKeypadInputDelegate::CleanupUinput()
        {
            if (mUinputFd >= 0)
            {
                ioctl(mUinputFd, UI_DEV_DESTROY);
                close(mUinputFd);
                mUinputFd = -1;
                ChipLogProgress(AppServer, "Uinput device cleaned up");
            }
        }

        void MatterKeypadInputDelegate::SendKeyEvent(int linuxKeyCode)
        {
            if (mUinputFd < 0)
            {
                ChipLogError(AppServer, "Uinput not initialized");
                return;
            }

            struct input_event ev;
            memset(&ev, 0, sizeof(ev));
            gettimeofday(&ev.time, NULL);

            // Key press
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 1;
            write(mUinputFd, &ev, sizeof(ev));

            // Sync
            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            // Small delay between press and release
            usleep(100);

            // Key release
            ev.type = EV_KEY;
            ev.code = linuxKeyCode;
            ev.value = 0;
            gettimeofday(&ev.time, NULL);
            write(mUinputFd, &ev, sizeof(ev));

            // Sync
            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;
            write(mUinputFd, &ev, sizeof(ev));

            ChipLogProgress(AppServer, "Sent key event: code=%d", linuxKeyCode);
        }

        int MatterKeypadInputDelegate::GetLinuxKeyCode(const char* keyName)
        {
            // Map RDK key names to Linux input key codes
            // Based on keySimulator's mapping table
            if (strcmp(keyName, "up") == 0) return KEY_UP;
            if (strcmp(keyName, "down") == 0) return KEY_DOWN;
            if (strcmp(keyName, "left") == 0) return KEY_LEFT;
            if (strcmp(keyName, "right") == 0) return KEY_RIGHT;
            if (strcmp(keyName, "select") == 0) return KEY_ENTER;
            if (strcmp(keyName, "exit") == 0) return KEY_ESC;
            if (strcmp(keyName, "guide") == 0) return KEY_HOME;
            if (strcmp(keyName, "settings") == 0) return KEY_SETUP;
            if (strcmp(keyName, "0") == 0) return KEY_0;
            if (strcmp(keyName, "1") == 0) return KEY_1;
            if (strcmp(keyName, "2") == 0) return KEY_2;
            if (strcmp(keyName, "3") == 0) return KEY_3;
            if (strcmp(keyName, "4") == 0) return KEY_4;
            if (strcmp(keyName, "5") == 0) return KEY_5;
            if (strcmp(keyName, "6") == 0) return KEY_6;
            if (strcmp(keyName, "7") == 0) return KEY_7;
            if (strcmp(keyName, "8") == 0) return KEY_8;
            if (strcmp(keyName, "9") == 0) return KEY_9;
            return -1;
        }

        void MatterKeypadInputDelegate::HandleSendKey(
            chip::app::CommandResponseHelper<KeypadInput::Commands::SendKeyResponse::Type> & helper,
            const KeypadInput::CECKeyCodeEnum & keyCode)
        {
            // Map Matter CEC key codes to keySimulator commands
            const char* keySimCmd = nullptr;

            switch (keyCode)
            {
                case KeypadInput::CECKeyCodeEnum::kUp:
                    keySimCmd = "up";
                    break;
                case KeypadInput::CECKeyCodeEnum::kDown:
                    keySimCmd = "down";
                    break;
                case KeypadInput::CECKeyCodeEnum::kLeft:
                    keySimCmd = "left";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRight:
                    keySimCmd = "right";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSelect:
                    keySimCmd = "select";
                    break;
                case KeypadInput::CECKeyCodeEnum::kBackward:
                case KeypadInput::CECKeyCodeEnum::kExit:
                    keySimCmd = "exit";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRootMenu:
                case KeypadInput::CECKeyCodeEnum::kContentsMenu:
                case KeypadInput::CECKeyCodeEnum::kFavoriteMenu:
                    keySimCmd = "guide";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSetupMenu:
                    keySimCmd = "settings";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumber0OrNumber10:
                    keySimCmd = "0";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers1:
                    keySimCmd = "1";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers2:
                    keySimCmd = "2";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers3:
                    keySimCmd = "3";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers4:
                    keySimCmd = "4";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers5:
                    keySimCmd = "5";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers6:
                    keySimCmd = "6";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers7:
                    keySimCmd = "7";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers8:
                    keySimCmd = "8";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers9:
                    keySimCmd = "9";
                    break;
                default:
                    break;
            }

            // Send success response immediately
            KeypadInput::Commands::SendKeyResponse::Type response;
            response.status = KeypadInput::StatusEnum::kSuccess;
            helper.Success(response);

            // Inject key directly via uinput (fast path - no process spawning)
            if (keySimCmd != nullptr)
            {
                int linuxKeyCode = GetLinuxKeyCode(keySimCmd);
                if (linuxKeyCode >= 0)
                {
                    SendKeyEvent(linuxKeyCode);
                    ChipLogProgress(AppServer, "Injected key: %s (linux code: %d)", keySimCmd, linuxKeyCode);
                }
                else
                {
                    ChipLogError(AppServer, "Failed to map key: %s", keySimCmd);
                }
            }
            else
            {
                ChipLogProgress(AppServer, "Key code %d not mapped", static_cast<uint8_t>(keyCode));
            }
        }

        uint32_t MatterKeypadInputDelegate::GetFeatureMap(chip::EndpointId endpoint)
        {
            // Enable all key features: NavigationKeyCodes, LocationKeys, NumberKeys
            return 0x07; // Bits 0,1,2 set
        }

        // ============================================================================
        // MatterApplicationLauncherDelegate Implementation
        // ============================================================================

        MatterApplicationLauncherDelegate::MatterApplicationLauncherDelegate()
        {
            ChipLogProgress(AppServer, "MatterApplicationLauncherDelegate created");
        }

        void MatterApplicationLauncherDelegate::HandleLaunchApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const chip::ByteSpan & data,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleLaunchApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // TODO: Integrate with Thunder application management
            // For now, return success
            response.status = ApplicationLauncher::StatusEnum::kSuccess;
            // response.data is Optional, leave it unset (no data to return)

            helper.Success(response);

            // TODO: Launch the application via Thunder plugin
            // Example: use rdkshell or residentapp to launch the specified application
            ChipLogProgress(AppServer, "Application launch would be executed here");
        }

        void MatterApplicationLauncherDelegate::HandleStopApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleStopApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // TODO: Integrate with Thunder application management
            response.status = ApplicationLauncher::StatusEnum::kSuccess;

            helper.Success(response);

            // TODO: Stop the application via Thunder plugin
            ChipLogProgress(AppServer, "Application stop would be executed here");
        }

        void MatterApplicationLauncherDelegate::HandleHideApp(
            chip::app::CommandResponseHelper<ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
            const ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application)
        {
            ChipLogProgress(AppServer, "HandleHideApp: catalogVendorId=%d, applicationId=%.*s",
                          application.catalogVendorID,
                          static_cast<int>(application.applicationID.size()),
                          application.applicationID.data());

            ApplicationLauncher::Commands::LauncherResponse::Type response;

            // TODO: Integrate with Thunder application management
            response.status = ApplicationLauncher::StatusEnum::kSuccess;

            helper.Success(response);

            // TODO: Hide/minimize the application via Thunder plugin
            ChipLogProgress(AppServer, "Application hide would be executed here");
        }        CHIP_ERROR MatterApplicationLauncherDelegate::HandleGetCatalogList(chip::app::AttributeValueEncoder & encoder)
        {
            // Return list of supported catalog vendor IDs
            // 0 = Content platform (CSA specification)
            return encoder.EncodeList([](const auto & listEncoder) -> CHIP_ERROR {
                // Add catalog vendor ID 0 (CSA specification)
                ReturnErrorOnFailure(listEncoder.Encode(static_cast<uint16_t>(0)));
                return CHIP_NO_ERROR;
            });
        }

        // ============================================================================
        // MatterClusterDelegateManager Implementation
        // ============================================================================

        MatterClusterDelegateManager& MatterClusterDelegateManager::GetInstance()
        {
            static MatterClusterDelegateManager instance;
            return instance;
        }

        void MatterClusterDelegateManager::Initialize()
        {
            if (mInitialized)
            {
                return;
            }

            // Create KeypadInput delegate
            mKeypadInputDelegate = std::make_unique<MatterKeypadInputDelegate>();

            // Register delegate for KeypadInput cluster on endpoint 3 (static configuration from ZAP)
            KeypadInput::SetDefaultDelegate(3, mKeypadInputDelegate.get());
            mRegisteredEndpoints.push_back(3);

            // Create ApplicationLauncher delegate
            mApplicationLauncherDelegate = std::make_unique<MatterApplicationLauncherDelegate>();

            // Register delegate for ApplicationLauncher cluster on endpoint 3
            ApplicationLauncher::SetDefaultDelegate(3, mApplicationLauncherDelegate.get());

            mInitialized = true;
            ChipLogProgress(AppServer, "KeypadInput and ApplicationLauncher delegates registered for endpoint 3");
        }



        void MatterClusterDelegateManager::Shutdown()
        {
            if (!mInitialized)
            {
                return;
            }

            // Unregister delegates
            for (chip::EndpointId ep : mRegisteredEndpoints)
            {
                KeypadInput::SetDefaultDelegate(ep, nullptr);
                ApplicationLauncher::SetDefaultDelegate(ep, nullptr);
            }
            mRegisteredEndpoints.clear();

            // Cleanup
            mKeypadInputDelegate.reset();
            mApplicationLauncherDelegate.reset();
            mInitialized = false;
        }

    } // namespace Plugin
} // namespace WPEFramework


