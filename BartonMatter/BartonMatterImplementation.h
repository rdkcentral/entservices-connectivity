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
#include <provider/barton-core-network-credentials-provider.h>

#include <mutex>
#include <thread>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#define B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER_TYPE (b_reference_network_credentials_provider_get_type())

G_DECLARE_FINAL_TYPE(BReferenceNetworkCredentialsProvider,
                     b_reference_network_credentials_provider,
                     B_REFERENCE,
                     NETWORK_CREDENTIALS_PROVIDER,
                     GObject);

BReferenceNetworkCredentialsProvider *b_reference_network_credentials_provider_new(void);
void b_reference_network_credentials_provider_set_wifi_network_credentials(const gchar *ssid, const gchar *password);

#ifdef __cplusplus
}
#endif
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
	    virtual Core::hresult CommissionDevice(const std::string passcode /* @in*/)override;

	    void InitializeClient(gchar *confDir);
	    static void SetDefaultParameters(BCoreInitializeParamsContainer *params);
	    bool Commission(BCoreClient *client, gchar *setupPayload,guint16 timeoutSeconds);
	    BEGIN_INTERFACE_MAP(BartonMatterImplementation)
            INTERFACE_ENTRY(Exchange::IBartonMatter)
            END_INTERFACE_MAP

        private:
	    BCoreClient *bartonClient;
	    static gchar* GetConfigDirectory();
        };
    } // namespace Plugin
} // namespace WPEFramework
