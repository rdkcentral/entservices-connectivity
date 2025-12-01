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

#include "Module.h"
#include "UtilsLogging.h"
#include <interfaces/IBartonMatter.h>
#include <interfaces/Ids.h>
#include <barton-core-client.h>
#include <barton-core-properties.h>
#include <provider/barton-core-network-credentials-provider.h>
#include <events/barton-core-endpoint-added-event.h>
#include <events/barton-core-device-added-event.h>
#include <mutex>
#include <thread>
#include <vector>

#include <access/AccessControl.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>
#include <app/app-platform/ContentAppPlatform.h>
#include <app/app-platform/ContentApp.h>
#include <app/server/Server.h>

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
            virtual Core::hresult SetWifiCredentials(const std::string ssid /* @in */, const std::string password /* @in */) override;
            virtual Core::hresult InitializeCommissioner() override;
            virtual Core::hresult CommissionDevice(const std::string passcode /* @in*/) override;
            virtual Core::hresult ReadResource(std::string uri /* @in*/, std::string resourceType /* @in*/, std::string &result /* @out*/)override;
            virtual Core::hresult WriteResource(std::string uri /* @in*/, std::string resourceType /* @in*/, std::string value /* @in*/)override;
            virtual Core::hresult ListDevices(std::string& deviceList /* @out */) override;

            void InitializeClient(gchar *confDir);
            static void SetDefaultParameters(BCoreInitializeParamsContainer *params);
            bool Commission(BCoreClient *client, gchar *setupPayload, guint16 timeoutSeconds);
	        static void DeviceAddedHandler(BCoreClient *source, BCoreDeviceAddedEvent *event, gpointer userData);
	        bool ConfigureClientACL(const std::string& deviceUuid, uint16_t vendorId, uint16_t productId);
	        bool AddACLEntryForClient(uint16_t vendorId, uint16_t productId, const std::string& deviceUuid);
            bool GetNodeIdFromDeviceUuid(const std::string& deviceUuid, uint64_t& nodeId);
	        static void DeviceConfigurationCompletedHandler(BCoreClient *client, const gchar *deviceUuid, gboolean success, gpointer userData);
            void OnSessionEstablished(const chip::SessionHandle & sessionHandle);
            void OnSessionFailure(const chip::ScopedNodeId & peerId, CHIP_ERROR error);
            static void OnSessionEstablishedStatic(void * context, chip::Messaging::ExchangeManager & exchangeMgr, const chip::SessionHandle & sessionHandle);
            static void OnSessionFailureStatic(void * context, const chip::ScopedNodeId & peerId, CHIP_ERROR error);


            static void EndpointAddedHandler(BCoreClient *source, BCoreEndpointAddedEvent *event, gpointer userData);
            BEGIN_INTERFACE_MAP(BartonMatterImplementation)
            INTERFACE_ENTRY(Exchange::IBartonMatter)
            END_INTERFACE_MAP

        private:
            BCoreClient *bartonClient; // Pointer to Barton Core client instance
            std::string savedDeviceUri; // Store the device URI from endpoint
            std::mutex deviceUriMtx; // Protect access to savedDeviceUri
            static gchar* GetConfigDirectory();
            chip::Callback::Callback<void (*)(void*, chip::Messaging::ExchangeManager&, const chip::SessionHandle&)> mSuccessCallback;
            chip::Callback::Callback<void (*)(void*, const chip::ScopedNodeId&, CHIP_ERROR)> mFailureCallback;

            // For scheduling session establishment
            uint64_t mEstablishSessionNodeId = 0;
            chip::FabricIndex mEstablishSessionFabricIndex = 0;
            static void EstablishSessionWork(intptr_t context);

            // Store client device information for ManageClientAccess
            std::string mClientDeviceUuid;
            uint16_t mClientVendorId = 0;
            uint16_t mClientProductId = 0;

            // Helper method to retrieve vendor/product IDs from device
            bool GetDeviceVendorProductIds(const std::string& deviceUuid, uint16_t& vendorId, uint16_t& productId);
        };
    } // namespace Plugin
} // namespace WPEFramework
