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
            ChipLogProgress(AppServer, "MatterKeypadInputDelegate::HandleSendKey called with keyCode=%d",
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
                case KeypadInput::CECKeyCodeEnum::kNumber0OrNumber10:
                    keyName = "Number 0/10";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers1:
                    keyName = "Number 1";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers2:
                    keyName = "Number 2";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers3:
                    keyName = "Number 3";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers4:
                    keyName = "Number 4";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers5:
                    keyName = "Number 5";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers6:
                    keyName = "Number 6";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers7:
                    keyName = "Number 7";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers8:
                    keyName = "Number 8";
                    break;
                case KeypadInput::CECKeyCodeEnum::kNumbers9:
                    keyName = "Number 9";
                    break;
                default:
                    break;
            }

            ChipLogProgress(AppServer, " KeypadInput: Received '%s' key press (code=%d)",
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
                ChipLogProgress(AppServer, "MatterClusterDelegateManager already initialized");
                return;
            }

            ChipLogProgress(AppServer, "Initializing Matter cluster delegates...");

            // Create one shared KeypadInput delegate (memory efficient)
            mKeypadInputDelegate = std::make_unique<MatterKeypadInputDelegate>();

            mInitialized = true;
            ChipLogProgress(AppServer, "Matter cluster delegates ready (will register via ZAP callbacks)");
        }

        void MatterClusterDelegateManager::RegisterKeypadInputDelegate(chip::EndpointId endpoint)
        {
            if (!mKeypadInputDelegate)
            {
                ChipLogError(AppServer, "Cannot register KeypadInput delegate - not initialized");
                return;
            }

            KeypadInput::SetDefaultDelegate(endpoint, mKeypadInputDelegate.get());
            mRegisteredEndpoints.push_back(endpoint);
            ChipLogProgress(AppServer, "Registered KeypadInput delegate for endpoint %u", endpoint);
        }

        void MatterClusterDelegateManager::Shutdown()
        {
            if (!mInitialized)
            {
                return;
            }

            ChipLogProgress(AppServer, "Shutting down Matter cluster delegates...");

            // Unregister delegate from all endpoints
            for (chip::EndpointId ep : mRegisteredEndpoints)
            {
                KeypadInput::SetDefaultDelegate(ep, nullptr);
            }
            mRegisteredEndpoints.clear();

            // Cleanup delegate instance
            mKeypadInputDelegate.reset();

            mInitialized = false;
            ChipLogProgress(AppServer, "Matter cluster delegates shutdown complete");
        }

    } // namespace Plugin
} // namespace WPEFramework

// ============================================================================
// ZAP Cluster Init Callbacks
// ============================================================================

/**
 * @brief ZAP callback invoked when KeypadInput cluster is initialized on an endpoint
 *
 * This is called automatically by the Matter SDK when a KeypadInput cluster
 * is initialized (either static from ZAP file or dynamic at runtime).
 *
 * @param endpoint The endpoint ID where KeypadInput cluster was initialized
 */
void emberAfKeypadInputClusterInitCallback(chip::EndpointId endpoint)
{
    ChipLogProgress(Zcl, "KeypadInput cluster initialized on endpoint %u", endpoint);
    WPEFramework::Plugin::MatterClusterDelegateManager::GetInstance().RegisterKeypadInputDelegate(endpoint);
}
