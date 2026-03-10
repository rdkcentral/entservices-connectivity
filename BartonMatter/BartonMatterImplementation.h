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
#include <barton-core-commissioning-info.h>
#include <provider/barton-core-network-credentials-provider.h>
#include <events/barton-core-endpoint-added-event.h>
#include <events/barton-core-device-added-event.h>
#include <events/barton-core-device-removed-event.h>
#include <events/barton-core-resource-updated-event.h>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

#include <access/AccessControl.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>
#include <app/server/Server.h>
#include <app/WriteClient.h>
#include <app-common/zap-generated/cluster-objects.h>

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
            virtual Core::hresult GetCommissionedDeviceInfo(std::string& deviceInfo /* @out */) override;
            virtual Core::hresult RemoveDevice(const std::string deviceUuid /* @in */) override;
            virtual Core::hresult OpenCommissioningWindow(const uint16_t timeoutSeconds /* @in */, std::string& commissioningInfo /* @out */) override;
	    virtual Core::hresult OnVoiceCommandReceived(const std::string& payload /* @in */) override;            virtual Core::hresult Register(Exchange::IBartonMatter::INotification* sink /* @in */) override;
            virtual Core::hresult Unregister(Exchange::IBartonMatter::INotification* sink /* @in */) override;
            void InitializeClient(gchar *confDir);
            static void SetDefaultParameters(BCoreInitializeParamsContainer *params);
            bool Commission(BCoreClient *client, gchar *setupPayload, guint16 timeoutSeconds);
	        static void DeviceAddedHandler(BCoreClient *source, BCoreDeviceAddedEvent *event, gpointer userData);
	        static void DeviceRemovedHandler(BCoreClient *source, BCoreDeviceRemovedEvent *event, gpointer userData);
	        bool ConfigureClientACL(const std::string& deviceUuid, uint16_t vendorId, uint16_t productId);
	        bool AddACLEntryForClient(uint16_t vendorId, uint16_t productId, const std::string& deviceUuid);
            bool GetNodeIdFromDeviceUuid(const std::string& deviceUuid, uint64_t& nodeId);
	        static void DeviceConfigurationCompletedHandler(BCoreClient *client, const gchar *deviceUuid, gboolean success, gpointer userData);
            static void ResourceUpdatedHandler(BCoreClient *source, BCoreResourceUpdatedEvent *event, gpointer userData);
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

            // Notification sinks registered by the plugin for event forwarding
            std::vector<Exchange::IBartonMatter::INotification*> mNotificationSinks;
            std::mutex mNotificationMtx;

            // Structure to hold commissioned device information
            struct DeviceInfo {
                std::string deviceClass;   // Device class from devicedb (e.g., "light", "plug")
                std::string deviceDriver;  // Device driver from devicedb (e.g., "matterLight")
                // All resource values from deviceEndpoints.1.resources (e.g., "label" -> "tanuj",
                // "isOn" -> "true", "currentLevel" -> "254", "colorXY" -> "24939,24701")
                std::map<std::string, std::string> resources;
            };

            // Cached device info: nodeId -> DeviceInfo
            std::map<std::string, DeviceInfo> commissionedDevicesCache;
            std::mutex devicesCacheMtx; // Protect access to device cache
            bool devicesCacheInitialized = false;

            // Helper methods for device info management
            void ScanDeviceDatabase();
            std::string ExtractModelFromDeviceFile(const std::string& filePath);
            void UpdateDeviceCache(const std::string& nodeId, const std::string& modelName);
            void RemoveDeviceFromCache(const std::string& nodeId);

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

            // Write bindings to client device (simplified ManageClientAccess)
            void WriteClientBindings(chip::Messaging::ExchangeManager & exchangeMgr,
                                    const chip::SessionHandle & sessionHandle,
                                    chip::NodeId localNodeId,
                                    const std::vector<chip::EndpointId> & endpoints);

            // ============================================================
            // Voice Command Processing System
            // ============================================================
            
            /**
             * @brief Structure representing a parsed voice command
             */
            struct VoiceCommand {
                enum class Action {
                    UNKNOWN,
                    TURN_ON,
                    TURN_OFF,
                    TOGGLE,
                    DIM,
                    BRIGHTEN,
                    SET_LEVEL,
                    SET_BRIGHTNESS,  ///< "set brightness to X%" — maps to currentLevel (0-254)
                    SET_COLOR        ///< "set color to red/blue/..." — maps to colorXY (CIE 1931)
                };
                
                Action action = Action::UNKNOWN;
                std::string deviceType;        // e.g., "light", "plug", "outlet"
                std::string deviceQualifier;   // e.g., "bedroom", "kitchen"
                int levelValue = -1;           // For SET_LEVEL / SET_BRIGHTNESS actions (0-100)
                std::string colorName;         // For SET_COLOR actions (e.g., "red", "blue")
                
                bool isValid() const {
                    return action != Action::UNKNOWN && !deviceType.empty();
                }
            };
            
            /**
             * @brief Structure representing a matched device from commissioned list
             */
            struct DeviceMatch {
                std::string nodeId;
                std::string model;
                int confidence = 0;  // Match confidence score (0-100)
                
                bool isValid() const {
                    return !nodeId.empty() && confidence > 0;
                }
            };
            
            /**
             * @brief Main handler that orchestrates voice command to device action
             * @param voiceCommand The natural language command from user
             * @return true if command was successfully processed and executed
             */
            bool ExecuteVoiceAction(const std::string& voiceCommand);
            
            /**
             * @brief Parse natural language command into structured VoiceCommand
             * @param text The voice command text
             * @return Parsed VoiceCommand structure
             */
            VoiceCommand ParseVoiceCommand(const std::string& text) const;
            
            /**
             * @brief Find best matching device from commissioned devices
             * @param command The parsed voice command
             * @param deviceInfo JSON string from GetCommissionedDeviceInfo()
             * @return DeviceMatch with nodeId and confidence score
             */
            DeviceMatch FindMatchingDevice(const VoiceCommand& command, const std::string& deviceInfo) const;
            
            /**
             * @brief Map voice action to Matter resource type and value
             * @param action The voice command action
             * @param resourceType Output parameter for Matter resource type
             * @param value Output parameter for resource value
             * @return true if mapping succeeded
             */
            bool MapActionToResource(VoiceCommand::Action action, int levelValue,
                                   const std::string& colorName,
                                   std::string& resourceType, std::string& value) const;
            
            /**
             * @brief Normalize text for matching (lowercase, trim, remove punctuation)
             * @param text Input text
             * @return Normalized text
             */
            std::string NormalizeText(const std::string& text) const;
            
            /**
             * @brief Calculate similarity score between two strings
             * @param str1 First string
             * @param str2 Second string
             * @return Similarity score (0-100)
             */
            int CalculateSimilarity(const std::string& str1, const std::string& str2) const;

            /**
             * @brief Convert a brightness percentage (0-100) to a Matter level value (0-254)
             * @param percentage Input percentage in range [0, 100]
             * @return Matter level in range [0, 254]
             */
            static int PercentageToMatterLevel(int percentage);

            /**
             * @brief Look up the CIE 1931 XY colour coordinates for a named colour
             *
             * Supported colours: red, green, blue, yellow, purple, cyan, white
             *
             * @param colorName Lowercase colour name
             * @return "x,y" string (e.g. "45712,18133") or empty string if unknown
             */
            static std::string GetColorXY(const std::string& colorName);

            /**
             * @brief Auto-retrieve WiFi credentials from NetworkManager connection files
             *
             * Scans /etc/NetworkManager/system-connections for .nmconnection files,
             * finds the first one with type=wifi, and extracts its ssid and psk.
             * Used as a fallback when the user has not called SetWifiCredentials().
             *
             * @param ssid Output parameter for the WiFi SSID
             * @param psk  Output parameter for the WiFi password
             * @return true if a valid WiFi connection with credentials was found
             */
            static bool RetrieveWifiCredentialsFromNM(std::string& ssid, std::string& psk);
        };
    } // namespace Plugin
} // namespace WPEFramework
