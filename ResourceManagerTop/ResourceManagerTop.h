/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include "Module.h"
#include "UtilsThreadRAII.h"
#include "UtilsLogging.h"
#include "UtilsJsonRpc.h"

namespace WPEFramework {
    namespace Plugin {
        class ResourceManagerTop : public PluginHost::IPlugin, public PluginHost::JSONRPC {
            private:
                // To Prevent Copy
                ResourceManagerTop(const ResourceManagerTop&) = delete;
                ResourceManagerTop& operator=(const ResourceManagerTop&) = delete; 

                //JSON-RPC Registered Methods
                uint32_t getApiVersionNumber(const JsonObject& parameters, JsonObject& response);
                uint32_t getSystemResourceInfo(const JsonObject& parameters, JsonObject& response);
                uint32_t killProcess(const JsonObject& parameters, JsonObject& response);
                uint32_t getState(const JsonObject& parameters, JsonObject& response);
            private:
                //Internal Logic
                string exec_top();
                bool kill_process(const int& pid);
                bool kill_process_by_name(const string& processName);
            public:
                //Service Name
                static const string SERVICE_NAME;

                //Methods
                static const string METHOD_GET_API_VERSION_NUMBER;
                static const string METHOD_GET_SYSTEM_RESOURCE_INFO;
                static const string METHOD_GET_STATE;

                ResourceManagerTop();
                virtual ~ResourceManagerTop();

                virtual const string Initialize(PluginHost::IShell* shell) override;
                virtual void Deinitialize(PluginHost::IShell* service) override;
                virtual string Information() const override;

                BEGIN_INTERFACE_MAP(ResourceManagerTop)
                INTERFACE_ENTRY(PluginHost::IPlugin)
                INTERFACE_ENTRY(PluginHost::IDispatcher)
                END_INTERFACE_MAP
            private:
                uint32_t m_apiVersionNumber;
                PluginHost::IShell* _service;

        }; // class ResourceManagerTop
    } // namespace Plugin
} // namespace WPEFramework