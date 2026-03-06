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

#include "BartonMatterPlugin.h"

const string WPEFramework::Plugin::BartonMatter::SERVICE_NAME = "org.rdk.BartonMatter";

using namespace std;

namespace WPEFramework {
    namespace Plugin {

        SERVICE_REGISTRATION(BartonMatter, 1, 0);

        BartonMatter* BartonMatter::_instance = nullptr;

        BartonMatter::BartonMatter()
            : PluginHost::IPlugin(), PluginHost::JSONRPC(), mService(nullptr), mBartonMatter(nullptr)
        {
            BartonMatter::_instance = this;
        }

        BartonMatter::~BartonMatter()
        {
        }

        const string BartonMatter::Initialize(PluginHost::IShell* service )
        {
            string message;

            ASSERT(mService == nullptr);
            ASSERT(mBartonMatter == nullptr);

            mConnectionId = 0;
            mService = service;

            mBartonMatter = mService->Root<Exchange::IBartonMatter>(mConnectionId, 5000, _T("BartonMatterImplementation"));

            if (mBartonMatter == nullptr)
            {
                message = _T("BartonMatter implementation could not be instantiated.");
                mService = nullptr;
                return message;
            }
            mConfig.FromString(service->ConfigLine());
	    Exchange::JBartonMatter::Register(*this, mBartonMatter);

            // Register ourselves as a notification sink so GLib signals from BartonCore
            // (resource-updated, device-added) are forwarded to us via COM-RPC and then
            // broadcast to subscribed JSON-RPC clients via sendNotify.
            mBartonMatter->Register(this);

            return "";
        }

        void BartonMatter::Deinitialize(PluginHost::IShell* /* service */)
        {
	    LOGINFO("Deinitializing BartonMatter instance");	
            if (mBartonMatter != nullptr) {

                mBartonMatter->Unregister(this);
                Exchange::JBartonMatter::Unregister(*this);

                mBartonMatter->Release();
                mBartonMatter = nullptr;
                mService = nullptr;
                BartonMatter::_instance = nullptr;
                LOGINFO("BartonMatter deinitialized successfully");

            }
	}

        string BartonMatter::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }

        void BartonMatter::OnDeviceCommissioned(const std::string& nodeId, const std::string& deviceClass)
        {
            LOGINFO("BartonMatter: broadcasting onDeviceCommissioned — nodeId=%s, deviceClass=%s",
                    nodeId.c_str(), deviceClass.c_str());
            JsonObject params;
            params["nodeId"]      = nodeId;
            params["deviceClass"] = deviceClass;
            sendNotify("onDeviceCommissioned", params);
        }

        void BartonMatter::OnDeviceStateChanged(const std::string& nodeId, const std::string& resourceType, const std::string& value)
        {
            LOGINFO("BartonMatter: broadcasting onDeviceStateChanged — nodeId=%s, resourceType=%s, value=%s",
                    nodeId.c_str(), resourceType.c_str(), value.c_str());
            JsonObject params;
            params["nodeId"]        = nodeId;
            params["resourceType"]  = resourceType;
            params["value"]         = value;
            sendNotify("onDeviceStateChanged", params);
        }

    } // namespace Plugin
} // namespace WPEFramework
