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
            return "";
        }

        void BartonMatter::Deinitialize(PluginHost::IShell* /* service */)
        {
	    LOGINFO("Deinitializing BartonMatter instance");	
            if (mBartonMatter != nullptr) {

                Exchange::JBartonMatter::Unregister(*this);
                mBartonMatter->Deinitialize();

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
        
    } // namespace Plugin
} // namespace WPEFramework
