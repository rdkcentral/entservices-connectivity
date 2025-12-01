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

#include "BartonClusterDelegates.h"
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app::Clusters;

namespace WPEFramework
{
    namespace Plugin
    {
        // Define supported key codes
        constexpr KeypadInput::CECKeyCodeEnum BartonKeypadInputDelegate::sSupportedKeyCodes[];

        BartonKeypadInputDelegate::BartonKeypadInputDelegate()
        {
            ChipLogProgress(AppServer, "BartonKeypadInputDelegate created");
        }

        void BartonKeypadInputDelegate::HandleSendKey(
            chip::app::CommandResponseHelper<KeypadInput::Commands::SendKeyResponse::Type> & helper,
            const KeypadInput::CECKeyCodeEnum & keyCode)
        {
            ChipLogProgress(AppServer, "BartonKeypadInputDelegate::HandleSendKey called with keyCode=%d",
                          static_cast<uint8_t>(keyCode));

            // TODO: Route to actual system key handler
            // For now, just log the key press
            const char* keyName = "Unknown";
            switch (keyCode)
            {
                case KeypadInput::CECKeyCodeEnum::kUp:
                    keyName = "Up";
                    break;
                case KeypadInput::CECKeyCodeEnum::kDown:
                    keyName = "Down";
                    break;
                case KeypadInput::CECKeyCodeEnum::kLeft:
                    keyName = "Left";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRight:
                    keyName = "Right";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSelect:
                    keyName = "Select/OK";
                    break;
                case KeypadInput::CECKeyCodeEnum::kBackward:
                    keyName = "Back";
                    break;
                case KeypadInput::CECKeyCodeEnum::kExit:
                    keyName = "Exit";
                    break;
                case KeypadInput::CECKeyCodeEnum::kRootMenu:
                    keyName = "Home/Root Menu";
                    break;
                case KeypadInput::CECKeyCodeEnum::kSetupMenu:
                    keyName = "Settings Menu";
                    break;
                case KeypadInput::CECKeyCodeEnum::kContentsMenu:
                    keyName = "Contents Menu";
                    break;
                case KeypadInput::CECKeyCodeEnum::kFavoriteMenu:
                    keyName = "Favorites";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumber0:
                case KeypadInput::CECKeyCodeEnum::kNumber1:
                case KeypadInput::CECKeyCodeEnum::kNumber2:
                case KeypadInput::CECKeyCodeEnum::kNumber3:
                case KeypadInput::CECKeyCodeEnum::kNumber4:
                case KeypadInput::CECKeyCodeEnum::kNumber5:
                case KeypadInput::CECKeyCodeEnum::kNumber6:
                case KeypadInput::CECKeyCodeEnum::kNumber7:
                case KeypadInput::CECKeyCodeEnum::kNumber8:
                case KeypadInput::CECKeyCodeEnum::kNumber9:
                    {
                        static char numStr[16];
                        snprintf(numStr, sizeof(numStr), "Number %d",
                                static_cast<uint8_t>(keyCode) - static_cast<uint8_t>(KeypadInput::CECKeyCodeEnum::kNumber0));
                        keyName = numStr;
                    }
                    break;
                default:
                    break;
            }

            ChipLogProgress(AppServer, "✅ KeypadInput: Received '%s' key press (code=%d)",
                          keyName, static_cast<uint8_t>(keyCode));

            // TODO: Forward to system input handler
            // Example: Send to RDK Input Manager, UINPUT, or other system service
            // For reference implementation, you would call:
            // - systemInputManager->SendKey(keyCode)
            // - or write to /dev/uinput
            // - or send IARM event

            // Send success response
            KeypadInput::Commands::SendKeyResponse::Type response;
            response.status = KeypadInput::StatusEnum::kSuccess;
            helper.Success(response);
        }

        uint32_t BartonKeypadInputDelegate::GetFeatureMap(chip::EndpointId endpoint)
        {
            // Enable all key features: NavigationKeyCodes, LocationKeys, NumberKeys
            return 0x07; // Bits 0,1,2 set
        }

        // ============================================================================
        // BartonClusterDelegateManager Implementation
        // ============================================================================

        BartonClusterDelegateManager& BartonClusterDelegateManager::GetInstance()
        {
            static BartonClusterDelegateManager instance;
            return instance;
        }

        void BartonClusterDelegateManager::Initialize()
        {
            if (mInitialized)
            {
                ChipLogProgress(AppServer, "BartonClusterDelegateManager already initialized");
                return;
            }

            ChipLogProgress(AppServer, "Initializing Barton cluster delegates...");

            // Create and register KeypadInput delegate for endpoints 1 and 3
            mKeypadInputDelegate = std::make_unique<BartonKeypadInputDelegate>();

            // Register for endpoint 1 (Video Player)
            KeypadInput::SetDefaultDelegate(1, mKeypadInputDelegate.get());
            ChipLogProgress(AppServer, "Registered KeypadInput delegate for endpoint 1 (Video Player)");

            // Register for endpoint 3 (Content App)
            KeypadInput::SetDefaultDelegate(3, mKeypadInputDelegate.get());
            ChipLogProgress(AppServer, "Registered KeypadInput delegate for endpoint 3 (Content App)");
            mInitialized = true;
            ChipLogProgress(AppServer, "All Barton cluster delegates initialized successfully");
        }

        void BartonClusterDelegateManager::Shutdown()
        {
            if (!mInitialized)
            {
                return;
            }

            ChipLogProgress(AppServer, "Shutting down Barton cluster delegates...");

            // Unregister delegates
            KeypadInput::SetDefaultDelegate(1, nullptr);
            KeypadInput::SetDefaultDelegate(3, nullptr);

            // Cleanup
            mKeypadInputDelegate.reset();

            mInitialized = false;
            ChipLogProgress(AppServer, "Barton cluster delegates shutdown complete");
        }

    } // namespace Plugin
} // namespace WPEFramework
