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
#include <cstdlib>

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

            // Execute keySimulator command
            KeypadInput::Commands::SendKeyResponse::Type response;
            if (keySimCmd != nullptr)
            {
                char command[128];
                snprintf(command, sizeof(command), "keySimulator -k%s", keySimCmd);
                ChipLogProgress(AppServer, "KeypadInput: Executing '%s'", keySimCmd);

                int result = system(command);
                if (result == 0)
                {
                    response.status = KeypadInput::StatusEnum::kSuccess;
                }
                else
                {
                    response.status = KeypadInput::StatusEnum::kSuccess;
                    ChipLogError(AppServer, "Command failed (result=%d)", result);
                }
            }
            else
            {
                response.status = KeypadInput::StatusEnum::kSuccess;
                ChipLogProgress(AppServer, "Key code %d not mapped", static_cast<uint8_t>(keyCode));
            }

            helper.Success(response);
        }

        uint32_t MatterKeypadInputDelegate::GetFeatureMap(chip::EndpointId endpoint)
        {
            // Enable all key features: NavigationKeyCodes, LocationKeys, NumberKeys
            return 0x07; // Bits 0,1,2 set
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

            mInitialized = true;
            ChipLogProgress(AppServer, "KeypadInput delegate registered for endpoint 3");
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
            }
            mRegisteredEndpoints.clear();

            // Cleanup
            mKeypadInputDelegate.reset();
            mInitialized = false;
        }

    } // namespace Plugin
} // namespace WPEFramework


