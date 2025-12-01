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

#include <app/clusters/keypad-input-server/keypad-input-server.h>

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
        class BartonKeypadInputDelegate : public chip::app::Clusters::KeypadInput::Delegate
        {
        public:
            BartonKeypadInputDelegate();
            virtual ~BartonKeypadInputDelegate() = default;

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
            static constexpr chip::app::Clusters::KeypadInput::CECKeyCodeEnum sSupportedKeyCodes[] = {
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kUp,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kDown,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kLeft,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kRight,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kSelect,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kBackward,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kExit,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kRootMenu,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kSetupMenu,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kContentsMenu,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kFavoriteMenu,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers3,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers4,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers5,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers6,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers7,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers8,
                chip::app::Clusters::KeypadInput::CECKeyCodeEnum::kNumbers9,
            };
        };

        /**
         * @brief Cluster delegate manager for Barton endpoints
         *
         * Manages registration and lifecycle of cluster delegates for all
         * Barton endpoints (video player, speaker, content app).
         */
        class BartonClusterDelegateManager
        {
        public:
            static BartonClusterDelegateManager& GetInstance();

            /**
             * @brief Initialize and register all cluster delegates
             *
             * Must be called after Matter stack initialization but before
             * accepting incoming connections.
             */
            void Initialize();

            /**
             * @brief Cleanup and unregister all cluster delegates
             */
            void Shutdown();

        private:
            BartonClusterDelegateManager() = default;
            ~BartonClusterDelegateManager() = default;
            BartonClusterDelegateManager(const BartonClusterDelegateManager&) = delete;
            BartonClusterDelegateManager& operator=(const BartonClusterDelegateManager&) = delete;

            bool mInitialized = false;

            // Delegate instances
            std::unique_ptr<BartonKeypadInputDelegate> mKeypadInputDelegate;
        };

    } // namespace Plugin
} // namespace WPEFramework
