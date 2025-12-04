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

#pragma once

#include <app/clusters/application-launcher-server/application-launcher-server.h>
#include <app/clusters/keypad-input-server/keypad-input-server.h>
#include <memory>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {
        /**
         * @brief KeypadInput delegate for handling remote control key commands
         *
         * Implements the Matter KeypadInput cluster delegate interface to handle
         * SendKey commands from casting clients. Routes key presses to the appropriate
         * system handlers.
         */
        class MatterKeypadInputDelegate : public chip::app::Clusters::KeypadInput::Delegate
        {
        public:
            MatterKeypadInputDelegate();
            virtual ~MatterKeypadInputDelegate();

            /**
             * @brief Handle incoming SendKey command
             *
             * @param helper CommandResponseHelper for sending response
             * @param keyCode The CECKeyCode value from the Matter specification
             */
            void HandleSendKey(chip::app::CommandResponseHelper<chip::app::Clusters::KeypadInput::Commands::SendKeyResponse::Type> & helper,
                             const chip::app::Clusters::KeypadInput::CECKeyCodeEnum & keyCode) override;

            /**
             * @brief Get feature map for the endpoint
             *
             * @param endpoint The endpoint ID
             * @return Feature map bits
             */
            uint32_t GetFeatureMap(chip::EndpointId endpoint) override;

        private:
            // Initialize/cleanup uinput device
            bool InitializeUinput();
            void CleanupUinput();

            // Send key event directly to uinput
            void SendKeyEvent(int linuxKeyCode);
            void SendKeyWithModifier(int modifierKeyCode, int mainKeyCode);

            // Map RDK key codes to Linux input key codes
            int GetLinuxKeyCode(const char* keyName);

            int mUinputFd = -1;  // File descriptor for /dev/uinput
        };

        /**
         * @brief ApplicationLauncher delegate for handling app launch commands
         *
         * Implements the Matter ApplicationLauncher cluster delegate interface to handle
         * LaunchApp, StopApp, and HideApp commands from casting clients. Routes commands to the
         * appropriate application management system.
         */
        class MatterApplicationLauncherDelegate : public chip::app::Clusters::ApplicationLauncher::Delegate
        {
        public:
            MatterApplicationLauncherDelegate();
            virtual ~MatterApplicationLauncherDelegate() = default;

            /**
             * @brief Handle incoming LaunchApp command
             *
             * @param helper CommandResponseHelper for sending response
             * @param data Application-specific data (not Nullable, just ByteSpan)
             * @param application The application to launch
             */
            void HandleLaunchApp(chip::app::CommandResponseHelper<chip::app::Clusters::ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
                               const chip::ByteSpan & data,
                               const chip::app::Clusters::ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application) override;

            /**
             * @brief Handle incoming StopApp command
             *
             * @param helper CommandResponseHelper for sending response
             * @param application The application to stop
             */
            void HandleStopApp(chip::app::CommandResponseHelper<chip::app::Clusters::ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
                             const chip::app::Clusters::ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application) override;

            /**
             * @brief Handle incoming HideApp command
             *
             * @param helper CommandResponseHelper for sending response
             * @param application The application to hide
             */
            void HandleHideApp(chip::app::CommandResponseHelper<chip::app::Clusters::ApplicationLauncher::Commands::LauncherResponse::Type> & helper,
                             const chip::app::Clusters::ApplicationLauncher::Structs::ApplicationStruct::DecodableType & application) override;

            /**
             * @brief Get the list of catalog vendor IDs
             *
             * @param encoder AttributeValueEncoder for the response
             */
            CHIP_ERROR HandleGetCatalogList(chip::app::AttributeValueEncoder & encoder) override;
        };

        /**
         * @brief Cluster delegate manager for Matter endpoints
         */
        class MatterClusterDelegateManager
        {
        public:
            static MatterClusterDelegateManager& GetInstance();

            void Initialize();
            void Shutdown();

        private:
            MatterClusterDelegateManager() = default;
            ~MatterClusterDelegateManager() = default;
            MatterClusterDelegateManager(const MatterClusterDelegateManager&) = delete;
            MatterClusterDelegateManager& operator=(const MatterClusterDelegateManager&) = delete;

            bool mInitialized = false;
            std::unique_ptr<MatterKeypadInputDelegate> mKeypadInputDelegate;
            std::unique_ptr<MatterApplicationLauncherDelegate> mApplicationLauncherDelegate;
            std::vector<chip::EndpointId> mRegisteredEndpoints;
        };

    } // namespace Plugin
} // namespace WPEFramework
