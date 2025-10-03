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
#include "UtilsLogging.h"
#include <interfaces/IBartonMatter.h>
#include <interfaces/Ids.h>
#include <barton-core-client.h>
#include <barton-core-properties.h>

#include <mutex>
#include <thread>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {

        class BartonMatterImplementation : public Exchange::IBartonMatter
        {
        public:
            BartonMatterImplementation();
            virtual ~BartonMatterImplementation();
	    virtual Core::hresult SetWifiCredentials(const std::string ssid /* @in */, const std::string password /* @in */)override;
	    virtual Core::hresult InitializeCommissioner()override;

	    static BCoreClient *InitializeClient(gchar *confDir);
	    static void SetDefaultParameters(BCoreInitializeParamsContainer *params);
	    BEGIN_INTERFACE_MAP(BartonMatterImplementation)
            INTERFACE_ENTRY(Exchange::IBartonMatter)
            END_INTERFACE_MAP

        private:
	    std::string wifiSSID;
	    std::string wifiPassword;
	    BCoreClient *bartonClient;
	    static gchar* GetConfigDirectory();

        };
    } // namespace Plugin
} // namespace WPEFramework
