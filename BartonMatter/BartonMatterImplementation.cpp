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

#include "BartonMatterImplementation.h"
#include "MatterClusterDelegates.h"
#include <fstream>
#include <dirent.h>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <vector>

using namespace std;

static gchar *network_ssid = NULL;
static gchar *network_psk = NULL;
std::mutex networkCredsMtx;

namespace WPEFramework
{
    namespace Plugin
    {
        std::string gPendingIdRequest("");
        std::string gPendingIdOptionsRequest("");
        std::string gPendingUrl("");
        SERVICE_REGISTRATION(BartonMatterImplementation, 1, 0);

        BartonMatterImplementation::BartonMatterImplementation()
            : bartonClient(nullptr),
            mSuccessCallback(BartonMatterImplementation::OnSessionEstablishedStatic, this),
            mFailureCallback(BartonMatterImplementation::OnSessionFailureStatic, this)
        {
            TRACE(Trace::Information, (_T("Constructing BartonMatterImplementation Service: %p"), this));
        }

        BartonMatterImplementation::~BartonMatterImplementation()
        {
            TRACE(Trace::Information, (_T("Destructing BartonMatterImplementation Service: %p"), this));

            // Shutdown cluster delegates
            MatterClusterDelegateManager::GetInstance().Shutdown();

            // Cleanup barton client if initialized
            if (bartonClient) {
                b_core_client_stop(bartonClient);
                g_object_unref(bartonClient);
                bartonClient = nullptr;
            }

            // Cleanup network credentials
            std::lock_guard<std::mutex> lock(networkCredsMtx);
            g_free(network_ssid);
            g_free(network_psk);
            network_ssid = nullptr;
            network_psk = nullptr;
        }
	/**
         * @brief Signal handler that fires when a device's configuration is completed
         *
         * This handler is called after Barton discovers a commissioned device's endpoints
         * but before the device is added to the device service. This is the ideal time to
         * create ACL entries for tv-casting-app devices, allowing them to access Barton's
         * endpoints to create bindings and send commands.
         *
         * @param client The BCoreClient instance
         * @param deviceUuid The UUID of the device that completed configuration
         * @param success Whether configuration was successful
         * @param userData Pointer to BartonMatterImplementation instance
         */
        void BartonMatterImplementation::DeviceConfigurationCompletedHandler(BCoreClient *client,
                                                                             const gchar *deviceUuid,
                                                                             gboolean success,
                                                                             gpointer userData)
        {
            LOGINFO("Device configuration completed event received!");
            LOGINFO("  Device UUID: %s", deviceUuid ? deviceUuid : "(null)");
            LOGINFO("  Success: %s", success ? "true" : "false");

            if (!userData || !deviceUuid || !success)
            {
                LOGERR("Invalid parameters or configuration failed in DeviceConfigurationCompletedHandler");
                return;
            }

            auto *instance = static_cast<BartonMatterImplementation*>(userData);

            // Configure ACL for the newly commissioned device
            // Using vendorId=0, productId=0 to allow any commissioned device to access Barton's endpoints
            // This is necessary for tv-casting-app to create bindings and send commands
            LOGINFO("Creating ACL entry for commissioned device %s", deviceUuid);
            bool aclSuccess = instance->ConfigureClientACL(deviceUuid, 0, 0);

            if (aclSuccess)
            {
                LOGINFO("Successfully configured ACL for device %s", deviceUuid);
            }
            else
            {
                LOGERR("Failed to configure ACL for device %s", deviceUuid);
            }
        }
        void BartonMatterImplementation::DeviceAddedHandler(BCoreClient *source, BCoreDeviceAddedEvent *event, gpointer userData)
        {
            LOGINFO("Device added event received - commissioning complete!");

            // Get properties directly from BCoreDeviceAddedEvent
            g_autofree gchar *deviceUuid = NULL;
            g_autofree gchar *deviceClass = NULL;
            g_object_get(G_OBJECT(event),
                         B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_UUID],
                         &deviceUuid,
                         B_CORE_DEVICE_ADDED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_ADDED_EVENT_PROP_DEVICE_CLASS],
                         &deviceClass,
                         NULL);            LOGWARN("Device added! UUID=%s, class=%s",
                    deviceUuid ? deviceUuid : "NULL",
                    deviceClass ? deviceClass : "NULL");

            // Configure ACL for all Matter-based devices (any deviceClass, as long as it's not null)
            // Matter devices include: castingVideoClient, matterCastingVideoClient, etc.
            if (deviceClass && deviceUuid) {
                BartonMatterImplementation* plugin = static_cast<BartonMatterImplementation*>(userData);
                if (plugin && deviceUuid) {
                    LOGWARN("=== DeviceAdded: Commissioning complete for %s ===", deviceUuid);
                    LOGWARN("Configuring ACL before client can initiate discovery...");

// Store commissioned device in cache with device class as initial model name
                    // The actual model name from devicedb will be loaded later
                    if (deviceClass) {
                        plugin->UpdateDeviceCache(std::string(deviceUuid), std::string(deviceClass));
                        LOGINFO("Cached commissioned device: %s with model: %s", deviceUuid, deviceClass);
                    }

                    // Configure ACL immediately after commissioning, before commissioned device
                    // can start its endpoint discovery sequence. This timing is critical:
                    // - DeviceAddedHandler fires synchronously in commissioning completion path
                    // - We create ACL here (takes microseconds)
                    // - Only then does control return and allow device to start discovery
                    bool aclResult = plugin->ConfigureClientACL(
                        std::string(deviceUuid),
                        0,  // vendorId: 0 means allow any vendor
                        0   // productId: 0 means allow any product
                    );

                    if (!aclResult) {
                        LOGERR("Failed to configure ACL for device %s", deviceUuid);
                    } else {
                        LOGWARN("=== ACL configured successfully - device %s can now discover endpoints ===", deviceUuid);
                    }

                    // Notify registered sinks — copy list under lock then invoke outside to avoid deadlocks
                    std::vector<Exchange::IBartonMatter::INotification*> sinks;
                    {
                        std::lock_guard<std::mutex> lock(plugin->mNotificationMtx);
                        sinks = plugin->mNotificationSinks;
                        for (auto* s : sinks) s->AddRef();
                    }
                    for (auto* s : sinks) {
                        s->OnDeviceCommissioned(std::string(deviceUuid), std::string(deviceClass));
                        s->Release();
                    }
                }
            }
        }

        void BartonMatterImplementation::DeviceRemovedHandler(BCoreClient *source, BCoreDeviceRemovedEvent *event, gpointer userData)
        {
            LOGINFO("Device removed event received - device disconnected!");

            // Get properties from BCoreDeviceRemovedEvent
            g_autofree gchar *deviceUuid = NULL;
            g_autofree gchar *deviceClass = NULL;
            g_object_get(G_OBJECT(event),
                         B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_UUID],
                         &deviceUuid,
                         B_CORE_DEVICE_REMOVED_EVENT_PROPERTY_NAMES[B_CORE_DEVICE_REMOVED_EVENT_PROP_DEVICE_CLASS],
                         &deviceClass,
                         NULL);

            LOGWARN("Device removed! UUID=%s, class=%s",
                    deviceUuid ? deviceUuid : "NULL",
                    deviceClass ? deviceClass : "NULL");

            // Remove device from cache
            if (deviceUuid) {
                BartonMatterImplementation* plugin = static_cast<BartonMatterImplementation*>(userData);
                if (plugin) {
                    plugin->RemoveDeviceFromCache(std::string(deviceUuid));
                    LOGINFO("Removed device from cache: %s", deviceUuid);
                }
            }
        }

        Core::hresult BartonMatterImplementation::SetWifiCredentials(const std::string ssid /* @in */, const std::string password /* @in */)
        {
            LOGWARN("BartonMatter: set wifi cred invoked");

            // Validate input parameters
            if (ssid.empty()) {
                LOGERR("Invalid SSID: cannot be empty");
                return (Core::ERROR_INVALID_INPUT_LENGTH);
            }

            if (password.empty()) {
                LOGERR("Invalid password: cannot be empty");
                return (Core::ERROR_INVALID_INPUT_LENGTH);
            }

            // Use the integrated network credentials provider
            b_reference_network_credentials_provider_set_wifi_network_credentials(ssid.c_str(), password.c_str());

            LOGWARN("BartonMatter wifi cred processed successfully ssid: %s | pass: %s", ssid.c_str(), password.c_str());
            return (Core::ERROR_NONE);
        }

        /**
         * @brief Scan /etc/NetworkManager/system-connections for a WiFi connection file
         *        and extract its SSID and PSK.
         *
         * Each .nmconnection file is an INI-style file.  The connection type is stored
         * as   type=<value>   in the [connection] section.  For WiFi connections the
         * SSID lives in the [wifi] section and the passphrase in [wifi-security].
         *
         * Only the credentials of the first valid WiFi profile found are returned so
         * that the function is deterministic when multiple WiFi profiles exist.
         */
        bool BartonMatterImplementation::RetrieveWifiCredentialsFromNM(std::string& ssid, std::string& psk)
        {
            static const char* NM_CONNECTIONS_DIR = "/etc/NetworkManager/system-connections";

            DIR* dir = opendir(NM_CONNECTIONS_DIR);
            if (!dir)
            {
                LOGERR("Cannot open NetworkManager connections directory: %s", NM_CONNECTIONS_DIR);
                return false;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                const std::string filename = entry->d_name;

                // Only process files ending in ".nmconnection" (13 chars)
                if (filename.size() < 14 ||
                    filename.compare(filename.size() - 13, 13, ".nmconnection") != 0)
                {
                    continue;
                }

                const std::string fullPath = std::string(NM_CONNECTIONS_DIR) + "/" + filename;
                std::ifstream file(fullPath);
                if (!file.is_open())
                {
                    LOGWARN("Cannot read connection file: %s", fullPath.c_str());
                    continue;
                }

                std::string currentSection;
                std::string fileType;
                std::string fileSsid;
                std::string filePsk;
                std::string line;

                while (std::getline(file, line))
                {
                    // Strip leading whitespace
                    const auto lstart = line.find_first_not_of(" \t\r\n");
                    if (lstart == std::string::npos) continue;
                    line = line.substr(lstart);

                    // Skip comments
                    if (line[0] == '#' || line[0] == ';') continue;

                    // Section header [section]
                    if (line[0] == '[')
                    {
                        const auto end = line.find(']');
                        if (end != std::string::npos)
                            currentSection = line.substr(1, end - 1);
                        continue;
                    }

                    // key=value pair
                    const auto eq = line.find('=');
                    if (eq == std::string::npos) continue;

                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);

                    // Trim trailing whitespace from key
                    const auto ktrim = key.find_last_not_of(" \t");
                    if (ktrim != std::string::npos) key.erase(ktrim + 1);

                    // Trim leading/trailing whitespace from value
                    const auto vstart = val.find_first_not_of(" \t");
                    if (vstart != std::string::npos) val = val.substr(vstart);
                    const auto vend = val.find_last_not_of(" \t\r\n");
                    if (vend != std::string::npos) val.erase(vend + 1);

                    if (currentSection == "connection" && key == "type")
                        fileType = val;
                    else if (currentSection == "wifi" && key == "ssid")
                        fileSsid = val;
                    else if (currentSection == "wifi-security" && key == "psk")
                        filePsk = val;
                }

                if (fileType == "wifi" && !fileSsid.empty() && !filePsk.empty())
                {
                    closedir(dir);
                    ssid = fileSsid;
                    psk  = filePsk;
                    LOGINFO("Auto-retrieved WiFi credentials from %s (SSID: %s)",
                            fullPath.c_str(), ssid.c_str());
                    return true;
                }
            }

            closedir(dir);
            LOGERR("No valid WiFi connection profile found in %s", NM_CONNECTIONS_DIR);
            return false;
        }

        Core::hresult BartonMatterImplementation::CommissionDevice(const std::string passcode)
        {
            LOGWARN("Commission called with passcode: %s", passcode.c_str());

            if (!bartonClient) {
                LOGERR("Barton client not initialized");
                return (Core::ERROR_GENERAL);
            }

            if (passcode.empty()) {
                LOGERR("Invalid passcode provided");
                return (Core::ERROR_INVALID_INPUT_LENGTH);
            }

            // If the user has not explicitly set WiFi credentials via SetWifiCredentials(),
            // attempt to auto-retrieve them from the NetworkManager connection profiles.
            // User-supplied credentials always take priority.
            {
                std::lock_guard<std::mutex> lock(networkCredsMtx);
                if (network_ssid && network_psk)
                {
                    LOGINFO("Using user-supplied WiFi credentials (SSID: %s)", network_ssid);
                }
            }

            bool needsAutoCredentials = false;
            {
                std::lock_guard<std::mutex> lock(networkCredsMtx);
                needsAutoCredentials = (!network_ssid || !network_psk);
            }

            if (needsAutoCredentials)
            {
                LOGWARN("WiFi credentials not set by caller — attempting auto-retrieval from NetworkManager");
                std::string autoSsid, autoPsk;
                if (RetrieveWifiCredentialsFromNM(autoSsid, autoPsk))
                {
                    Core::hresult setResult = SetWifiCredentials(autoSsid, autoPsk);
                    if (setResult != Core::ERROR_NONE)
                    {
                        LOGERR("Failed to apply auto-retrieved WiFi credentials");
                        return setResult;
                    }
                    LOGWARN("Auto-applied WiFi credentials for SSID: %s", autoSsid.c_str());
                }
                else
                {
                    LOGERR("Could not retrieve WiFi credentials from NetworkManager; "
                           "commissioning may fail without valid network credentials");
                }
            }

            g_autofree gchar* setupPayload = g_strdup(passcode.c_str());
            bool result = Commission(bartonClient, setupPayload, 120);

            return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }
        Core::hresult BartonMatterImplementation::ReadResource(std::string uri /* @in*/, std::string resourceType /* @in*/, std::string &result /* @out*/)
        {
            std::string fullUri;

            // Construct URI by directly appending resourceType: /uri/ep/1/r/resourceType
            fullUri = "/" + uri + "/ep/1/r/" + resourceType;

            g_autoptr(GError) err = NULL;
            g_autofree gchar *value = b_core_client_read_resource(bartonClient, fullUri.c_str(), &err);

            if(err == NULL && value != NULL)
            {
                LOGWARN("Read resource successful: %s = %s", fullUri.c_str(), value);
                result = std::string(value);
                return Core::ERROR_NONE;
            }
            else
            {
                LOGERR("Read resource failed for %s: %s", fullUri.c_str(), err ? err->message : "Unknown error");
                result = "";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult BartonMatterImplementation::WriteResource(std::string uri /* @in*/, std::string resourceType /* @in*/, std::string value /* @in*/)
        {
            std::string fullUri;
            bool result = true;

            // Construct URI by directly appending resourceType: /uri/ep/1/r/resourceType
            fullUri = "/" + uri + "/ep/1/r/" + resourceType;
            LOGWARN("Writing %s resource with value: %s", resourceType.c_str(), value.c_str());

            if(!b_core_client_write_resource(bartonClient, fullUri.c_str(), value.c_str()))
            {
                LOGERR("Write resource failed for URI: %s", fullUri.c_str());
                result = false;
            }
            else
            {
                LOGWARN("Write resource successful for URI: %s", fullUri.c_str());
            }
            return result ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        bool BartonMatterImplementation::Commission(BCoreClient *client, gchar *setupPayload, guint16 timeoutSeconds)
        {
            bool rc = true;
            g_autoptr(GError) error = NULL;
            rc = b_core_client_commission_device(client, setupPayload, timeoutSeconds, &error);
            if(rc)
            {
                LOGWARN("Attempting to commission device");
            }
            else
            {
                if(error != NULL && error->message != NULL)
                {
                    LOGWARN("Failed to commission device: %s", error->message);
                }
                else
                {
                    LOGWARN("Failed to commission device: Unknown error");
                }
            }
            return rc;
        }

        Core::hresult BartonMatterImplementation::Register(Exchange::IBartonMatter::INotification* sink)
        {
            ASSERT(sink != nullptr);
            std::lock_guard<std::mutex> lock(mNotificationMtx);
            // Guard against double-registration
            auto it = std::find(mNotificationSinks.begin(), mNotificationSinks.end(), sink);
            if (it == mNotificationSinks.end()) {
                sink->AddRef();
                mNotificationSinks.push_back(sink);
                LOGINFO("BartonMatter: registered notification sink %p (total: %zu)", sink, mNotificationSinks.size());
            } else {
                LOGWARN("BartonMatter: notification sink %p already registered", sink);
            }
            return Core::ERROR_NONE;
        }

        Core::hresult BartonMatterImplementation::Unregister(Exchange::IBartonMatter::INotification* sink)
        {
            ASSERT(sink != nullptr);
            std::lock_guard<std::mutex> lock(mNotificationMtx);
            auto it = std::find(mNotificationSinks.begin(), mNotificationSinks.end(), sink);
            if (it != mNotificationSinks.end()) {
                (*it)->Release();
                mNotificationSinks.erase(it);
                LOGINFO("BartonMatter: unregistered notification sink %p (remaining: %zu)", sink, mNotificationSinks.size());
            } else {
                LOGWARN("BartonMatter: notification sink %p not found for unregister", sink);
            }
            return Core::ERROR_NONE;
        }

        void BartonMatterImplementation::ResourceUpdatedHandler(BCoreClient *source, BCoreResourceUpdatedEvent *event, gpointer userData)
        {
            // Extract the BCoreResource object from the event
            g_autoptr(BCoreResource) resource = NULL;
            g_object_get(G_OBJECT(event),
                         B_CORE_RESOURCE_UPDATED_EVENT_PROPERTY_NAMES[B_CORE_RESOURCE_UPDATED_EVENT_PROP_RESOURCE],
                         &resource,
                         NULL);

            if (!resource) {
                LOGERR("ResourceUpdatedHandler: received NULL resource in event");
                return;
            }

            g_autofree gchar *deviceUuid  = NULL;
            g_autofree gchar *resourceId  = NULL;
            g_autofree gchar *value       = NULL;

            g_object_get(G_OBJECT(resource),
                         B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_DEVICE_UUID], &deviceUuid,
                         B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_ID],          &resourceId,
                         B_CORE_RESOURCE_PROPERTY_NAMES[B_CORE_RESOURCE_PROP_VALUE],       &value,
                         NULL);

            if (!deviceUuid || !resourceId || !value) {
                LOGERR("ResourceUpdatedHandler: missing required resource properties (uuid=%s, id=%s, value=%s)",
                       deviceUuid ? deviceUuid : "NULL",
                       resourceId ? resourceId : "NULL",
                       value      ? value      : "NULL");
                return;
            }

            LOGINFO("ResourceUpdatedHandler: device=%s, resource=%s, value=%s", deviceUuid, resourceId, value);

            BartonMatterImplementation* self = static_cast<BartonMatterImplementation*>(userData);
            if (!self) return;

            // Copy sink list under lock, then invoke callbacks outside lock to prevent deadlocks
            std::vector<Exchange::IBartonMatter::INotification*> sinks;
            {
                std::lock_guard<std::mutex> lock(self->mNotificationMtx);
                sinks = self->mNotificationSinks;
                for (auto* s : sinks) s->AddRef();
            }
            for (auto* s : sinks) {
                s->OnDeviceStateChanged(std::string(deviceUuid), std::string(resourceId), std::string(value));
                s->Release();
            }
        }


        void BartonMatterImplementation::InitializeClient(gchar *confDir)
        {
            g_autoptr(BCoreInitializeParamsContainer) params = b_core_initialize_params_container_new();
            b_core_initialize_params_container_set_storage_dir(params, confDir);
            g_autofree gchar* matterConfDir = g_strdup((std::string(confDir) + "/matter").c_str());
            g_mkdir_with_parents(matterConfDir, 0755);
            b_core_initialize_params_container_set_matter_storage_dir(params, matterConfDir);
            b_core_initialize_params_container_set_matter_attestation_trust_store_dir(params, matterConfDir);
            b_core_initialize_params_container_set_account_id(params, "1");
            g_autoptr(BReferenceNetworkCredentialsProvider) networkCredentialsProvider = b_reference_network_credentials_provider_new();
            b_core_initialize_params_container_set_network_credentials_provider(params, B_CORE_NETWORK_CREDENTIALS_PROVIDER(networkCredentialsProvider));

            bartonClient = b_core_client_new(params);
            BCorePropertyProvider *propProvider = b_core_initialize_params_container_get_property_provider(params);
            if(propProvider != NULL)
            {
                b_core_property_provider_set_property_string(propProvider, "device.subsystem.disable", "thread,zigbee");
            }

            // Connect device added signal handler - this is where we create ACLs
            g_signal_connect(bartonClient, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_ADDED, G_CALLBACK(DeviceAddedHandler), this);

            // Connect device removed signal handler - this is where we remove from cache
            g_signal_connect(bartonClient, B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REMOVED, G_CALLBACK(DeviceRemovedHandler), this);

            // Connect endpoint added signal handler
            g_signal_connect(bartonClient, B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED, G_CALLBACK(EndpointAddedHandler), this);

            // Connect resource updated signal handler - fires on any device attribute change
            g_signal_connect(bartonClient, B_CORE_CLIENT_SIGNAL_NAME_RESOURCE_UPDATED, G_CALLBACK(ResourceUpdatedHandler), this);

            SetDefaultParameters(params);
        }

        void BartonMatterImplementation::SetDefaultParameters(BCoreInitializeParamsContainer *params)
        {
            BCorePropertyProvider *propProvider =
                b_core_initialize_params_container_get_property_provider(params);
            if (propProvider != NULL)
            {
                // Set Matter's Device Instance Info details
                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_VENDOR_NAME, "Barton");

                b_core_property_provider_set_property_uint16(
                        propProvider, B_CORE_BARTON_MATTER_VENDOR_ID, 0xFFF1);

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_PRODUCT_NAME, "Barton Device");

                b_core_property_provider_set_property_uint16(
                        propProvider, B_CORE_BARTON_MATTER_PRODUCT_ID, 0x5678);

                b_core_property_provider_set_property_uint16(
                        propProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION, 1);

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_HARDWARE_VERSION_STRING, "Barton Hardware Version 1.0");

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_PART_NUMBER, "Barton-Part-001");

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_PRODUCT_URL, "https://www.example.com/device");

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_PRODUCT_LABEL, "Barton Device Label");

                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_SERIAL_NUMBER, "SN-123456789");

                // Set Manufacturing Date to "now"
                time_t now = time(NULL);
                struct tm *tm = localtime(&now);
                gchar manufacturingDate[11]; // YYYY-MM-DD format
                strftime(manufacturingDate, sizeof(manufacturingDate), "%Y-%m-%d", tm);
                b_core_property_provider_set_property_string(
                        propProvider, B_CORE_BARTON_MATTER_MANUFACTURING_DATE, manufacturingDate);

                // set default discriminator if not already set
                guint16 discriminator = b_core_property_provider_get_property_as_uint16(
                        propProvider, B_CORE_BARTON_MATTER_SETUP_DISCRIMINATOR, 0);
                if (discriminator == 0)
                {
                    // Use the well-known development discriminator 3840
                    b_core_property_provider_set_property_uint16(
                            propProvider, B_CORE_BARTON_MATTER_SETUP_DISCRIMINATOR, 3840);
                }

                // set default passcode if not already set
                guint32 passcode = b_core_property_provider_get_property_as_uint32(
                        propProvider, B_CORE_BARTON_MATTER_SETUP_PASSCODE, 0);
                if (passcode == 0)
                {
                    // Use the well-known development passcode 20202021
                    b_core_property_provider_set_property_uint32(
                            propProvider, B_CORE_BARTON_MATTER_SETUP_PASSCODE, 20202021);
                }
            }
        }

        Core::hresult BartonMatterImplementation::InitializeCommissioner()
        {
            // Check if both credentials are unset and provide defaults if so
            bool needsDefaults = false;
            {
                std::lock_guard<std::mutex> lock(networkCredsMtx);
                needsDefaults = (!network_ssid && !network_psk);
            }

            if (needsDefaults) {
                LOGWARN("WiFi credentials not set — attempting auto-retrieval from NetworkManager");
                std::string autoSsid, autoPsk;
                if (RetrieveWifiCredentialsFromNM(autoSsid, autoPsk))
                {
                    b_reference_network_credentials_provider_set_wifi_network_credentials(
                        autoSsid.c_str(), autoPsk.c_str());
                    LOGWARN("Auto-set WiFi credentials for SSID: %s", autoSsid.c_str());
                }
                else
                {
                    LOGWARN("Could not auto-retrieve WiFi credentials; using placeholder defaults");
                    b_reference_network_credentials_provider_set_wifi_network_credentials("MySSID", "MyPassword");
                }
            }

            g_autofree gchar* confDir = GetConfigDirectory();
            InitializeClient(confDir);

            if(!bartonClient)
            {
                LOGERR("Barton client not initialized");
                return (Core::ERROR_GENERAL);
            }

            g_autoptr(GError) error = NULL;
            if (!b_core_client_start(bartonClient)) {
                LOGERR("Failed to start Barton client");
                return (Core::ERROR_GENERAL);
            }

            b_core_client_set_system_property(bartonClient, "deviceDescriptorBypass", "true");

            // Initialize cluster delegates for handling incoming commands
            // Must run on Matter event loop to ensure proper timing
            chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
                ChipLogProgress(AppServer, "Scheduling cluster delegate initialization...");
                MatterClusterDelegateManager::GetInstance().Initialize();
            });

            LOGINFO("BartonMatter Commissioner initialized successfully");

            return (Core::ERROR_NONE);
        }

        gchar* BartonMatterImplementation::GetConfigDirectory()
        {
            const std::string pathStr = "/opt/.brtn-ds";
            g_mkdir_with_parents(pathStr.c_str(), 0755);
            return g_strdup(pathStr.c_str()); // Caller must free with g_free()
        }
        void BartonMatterImplementation::EndpointAddedHandler(BCoreClient *source, BCoreEndpointAddedEvent *event, gpointer userData)
        {
            LOGINFO("Endpoint added event received");

            g_autoptr(BCoreEndpoint) endpoint = NULL;
            g_object_get(
                G_OBJECT(event),
                B_CORE_ENDPOINT_ADDED_EVENT_PROPERTY_NAMES[B_CORE_ENDPOINT_ADDED_EVENT_PROP_ENDPOINT],
                &endpoint,
                NULL);

            g_return_if_fail(endpoint != NULL);

            g_autofree gchar *deviceUuid = NULL;
            g_autofree gchar *id = NULL;
            g_autofree gchar *uri = NULL;
            g_autofree gchar *profile = NULL;
            guint profileVersion = 0;

            g_object_get(G_OBJECT(endpoint),
                         B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_DEVICE_UUID],
                         &deviceUuid,
                         B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_ID],
                         &id,
                         B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_URI],
                         &uri,
                         B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE],
                         &profile,
                         B_CORE_ENDPOINT_PROPERTY_NAMES[B_CORE_ENDPOINT_PROP_PROFILE_VERSION],
                         &profileVersion,
                         NULL);

            LOGWARN("Endpoint added! deviceUuid=%s, id=%s, uri=%s, profile=%s, profileVersion=%d",
                    deviceUuid ? deviceUuid : "NULL",
                    id ? id : "NULL",
                    uri ? uri : "NULL",
                    profile ? profile : "NULL",
                    profileVersion);

            // Get the plugin instance from userData if needed for further processing
            BartonMatterImplementation* plugin = static_cast<BartonMatterImplementation*>(userData);
            if (plugin) {
                std::lock_guard<std::mutex> lock(plugin->deviceUriMtx);
                plugin->savedDeviceUri = std::string(uri);
                LOGINFO("Saved device URI: %s", plugin->savedDeviceUri.c_str());
            }
        }

        /**
         * @brief Configure ACL entry for a commissioned device to allow it to access Barton's endpoints
         *
         * This method creates an ACL entry that grants the commissioned device (identified by deviceUuid)
         * the ability to read Barton's endpoints, create bindings, and send commands. This is essential
         * for tv-casting-app to function properly after commissioning.
         *
         * @param deviceUuid The Matter node ID of the commissioned device (in hex string format)
         * @param vendorId Vendor ID to filter (0 = allow any vendor)
         * @param productId Product ID to filter (0 = allow any product)
         * @return true if ACL was created successfully, false otherwise
         */
        bool BartonMatterImplementation::ConfigureClientACL(const std::string& deviceUuid, uint16_t vendorId, uint16_t productId)
        {
            LOGINFO("ConfigureClientACL called for device %s (vendorId=0x%04x, productId=0x%04x)",
                    deviceUuid.c_str(), vendorId, productId);

            if (deviceUuid.empty())
            {
                LOGERR("ConfigureClientACL: Invalid empty deviceUuid");
                return false;
            }

            // Create ACL entry using Matter SDK
            bool result = AddACLEntryForClient(vendorId, productId, deviceUuid);

            if (result)
            {
                LOGINFO("Successfully configured ACL for device %s", deviceUuid.c_str());
            }
            else
            {
                LOGERR("Failed to configure ACL for device %s", deviceUuid.c_str());
            }

            return result;
        }

        /**
         * @brief Add an ACL entry using Matter SDK APIs
         *
         * Creates an Access Control List entry that allows a specific node (identified by deviceUuid)
         * to access all of Barton's clusters with Operate privilege. This uses the Matter SDK's
         * AccessControl APIs to directly manipulate the ACL table.
         *
         * @param vendorId Vendor ID filter (currently unused, 0 = any)
         * @param productId Product ID filter (currently unused, 0 = any)
         * @param deviceUuid The Matter node ID in hex string format (e.g., "90034FD9068DFF14")
         * @return true if ACL entry was created successfully, false otherwise
         */
        bool BartonMatterImplementation::AddACLEntryForClient(uint16_t vendorId, uint16_t productId, const std::string& deviceUuid)
        {
            using namespace chip;
            using namespace chip::Access;

            LOGINFO("AddACLEntryForClient: Creating ACL for device %s", deviceUuid.c_str());

            // Convert deviceUuid (hex string) to numeric node ID
            uint64_t nodeId = 0;
            if (!GetNodeIdFromDeviceUuid(deviceUuid, nodeId))
            {
                LOGERR("AddACLEntryForClient: Failed to convert deviceUuid to node ID");
                return false;
            }

            LOGINFO("AddACLEntryForClient: Converted deviceUuid %s to nodeId 0x%016llx",
                    deviceUuid.c_str(), (unsigned long long)nodeId);

            // Get our fabric index (we should be on fabric 1 after initialization)
            FabricIndex fabricIndex = 1;
            LOGINFO("AddACLEntryForClient: Using fabric index %d", fabricIndex);

            // Prepare the ACL entry
            AccessControl::Entry entry;
            CHIP_ERROR err = GetAccessControl().PrepareEntry(entry);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: PrepareEntry failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            // Set fabric index
            err = entry.SetFabricIndex(fabricIndex);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: SetFabricIndex failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            // Set privilege to Administer (full access for testing)
            err = entry.SetPrivilege(Privilege::kAdminister);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: SetPrivilege failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            // Set auth mode to CASE (Certificate Authenticated Session Establishment)
            err = entry.SetAuthMode(AuthMode::kCase);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: SetAuthMode failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            // Add the subject (the node that gets access)
            err = entry.AddSubject(nullptr, nodeId);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: AddSubject failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            /**
             * Following ManageClientAccess pattern, create a single ACL entry containing:
             * a) Video Player endpoint (endpoint 1) - all clusters
             * b) Speaker endpoint (endpoint 2) - all clusters
             * c) Content App endpoint (endpoint 3) - all clusters
             * d) Single subject which is the casting app
             *
             * Barton has these endpoints per barton.zap:
             * - Endpoint 0: Root device
             * - Endpoint 1: Video player (MA-videoplayer)
             * - Endpoint 2: Speaker (MA-speaker)
             * - Endpoint 3: Content application (MA-contentapplication)
             */

            ChipLogProgress(AppServer, "Create video player endpoint ACL target");
            // Add target for endpoint 1 (Video Player)
            {
                AccessControl::Entry::Target target = {
                    .flags = AccessControl::Entry::Target::kEndpoint,
                    .endpoint = 1  // Video player endpoint
                };
                err = entry.AddTarget(nullptr, target);
                if (err != CHIP_NO_ERROR)
                {
                    LOGERR("AddACLEntryForClient: AddTarget for video player endpoint failed: 0x%08lx", (unsigned long)err.AsInteger());
                    return false;
                }
            }

            ChipLogProgress(AppServer, "Create speaker endpoint ACL target");
            // Add target for endpoint 2 (Speaker)
            {
                AccessControl::Entry::Target target = {
                    .flags = AccessControl::Entry::Target::kEndpoint,
                    .endpoint = 2  // Speaker endpoint
                };
                err = entry.AddTarget(nullptr, target);
                if (err != CHIP_NO_ERROR)
                {
                    LOGERR("AddACLEntryForClient: AddTarget for speaker endpoint failed: 0x%08lx", (unsigned long)err.AsInteger());
                    return false;
                }
            }

            ChipLogProgress(AppServer, "Create content app endpoint ACL target");
            // Add target for endpoint 3 (Content App)
            {
                AccessControl::Entry::Target target = {
                    .flags = AccessControl::Entry::Target::kEndpoint,
                    .endpoint = 3  // Content app endpoint
                };
                err = entry.AddTarget(nullptr, target);
                if (err != CHIP_NO_ERROR)
                {
                    LOGERR("AddACLEntryForClient: AddTarget for content app endpoint failed: 0x%08lx", (unsigned long)err.AsInteger());
                    return false;
                }
            }

            // Create the entry in the ACL table
            // Note: The Matter SDK's CreateEntry signature is:
            // CreateEntry(const SubjectDescriptor *subjectDescriptor, FabricIndex fabricIndex,
            //             const Target *target, const Entry &entry)
            err = GetAccessControl().CreateEntry(nullptr, fabricIndex, nullptr, entry);
            if (err != CHIP_NO_ERROR)
            {
                LOGERR("AddACLEntryForClient: CreateEntry failed: 0x%08lx", (unsigned long)err.AsInteger());
                return false;
            }

            LOGINFO("AddACLEntryForClient: Successfully created ACL entry with 3 endpoints for node 0x%016llx on fabric %d",
                    (unsigned long long)nodeId, fabricIndex);

            // Store nodeId, fabricIndex, and deviceUuid in member variables for use in static work function
            this->mEstablishSessionNodeId = nodeId;
            this->mEstablishSessionFabricIndex = fabricIndex;
            this->mClientDeviceUuid = deviceUuid;
            chip::DeviceLayer::PlatformMgr().ScheduleWork(&BartonMatterImplementation::EstablishSessionWork, reinterpret_cast<intptr_t>(this));
            return true;
            }
        // Static work function for scheduling session establishment on the Matter event loop
        void BartonMatterImplementation::EstablishSessionWork(intptr_t context)
        {
            auto *self = reinterpret_cast<BartonMatterImplementation *>(context);
            chip::Server & server = chip::Server::GetInstance();
            chip::ScopedNodeId peerNode(self->mEstablishSessionNodeId, self->mEstablishSessionFabricIndex);
            server.GetCASESessionManager()->FindOrEstablishSession(peerNode, &self->mSuccessCallback, &self->mFailureCallback);
        }

        // Write bindings to client device using WriteClient
        void BartonMatterImplementation::WriteClientBindings(
            chip::Messaging::ExchangeManager & exchangeMgr,
            const chip::SessionHandle & sessionHandle,
            chip::NodeId localNodeId,
            const std::vector<chip::EndpointId> & endpoints)
        {
            using namespace chip;
            using namespace chip::app;
            using namespace chip::app::Clusters::Binding;

            ChipLogProgress(AppServer, "WriteClientBindings called for %zu endpoints", endpoints.size());
            ChipLogProgress(AppServer, "Local Node ID: 0x" ChipLogFormatX64, ChipLogValueX64(localNodeId));
            ChipLogProgress(AppServer, "Target Node ID: 0x" ChipLogFormatX64,
                          ChipLogValueX64(sessionHandle->GetPeer().GetNodeId()));

            // Build binding list for all accessible endpoints
            std::vector<Structs::TargetStruct::Type> bindings;

            for (chip::EndpointId endpoint : endpoints)
            {
                bindings.push_back(Structs::TargetStruct::Type{
                    .node        = MakeOptional(localNodeId),
                    .group       = NullOptional,
                    .endpoint    = MakeOptional(endpoint),
                    .cluster     = NullOptional,
                    .fabricIndex = chip::kUndefinedFabricIndex,
                });
            }

            ChipLogProgress(AppServer, "Writing %zu bindings to client", bindings.size());

            // Create WriteClient for attribute write
            auto writeClient = new chip::app::WriteClient(
                &exchangeMgr, nullptr, chip::Optional<uint16_t>::Missing());

            if (writeClient == nullptr)
            {
                ChipLogError(AppServer, "Failed to allocate WriteClient");
                return;
            }

            // Set up attribute path for Binding cluster's Binding attribute
            chip::app::AttributePathParams attributePathParams;
            attributePathParams.mEndpointId = 1; // Client device endpoint
            attributePathParams.mClusterId = chip::app::Clusters::Binding::Id;
            attributePathParams.mAttributeId = chip::app::Clusters::Binding::Attributes::Binding::Id;

            // Encode the binding list attribute
            chip::app::Clusters::Binding::Attributes::Binding::TypeInfo::Type bindingListAttr(bindings.data(), bindings.size());
            CHIP_ERROR err = writeClient->EncodeAttribute(attributePathParams, bindingListAttr);

            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(AppServer, "Failed to encode Binding attribute: %s", ErrorStr(err));
                delete writeClient;
                return;
            }

            // Send the write request
            err = writeClient->SendWriteRequest(sessionHandle);

            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(AppServer, "Failed to send write request: %s", ErrorStr(err));
                delete writeClient;
                return;
            }

            ChipLogProgress(AppServer, "Successfully sent binding write request to client");
            // Note: writeClient will be deleted in the callback
        }


        void BartonMatterImplementation::OnSessionEstablishedStatic(void * context, chip::Messaging::ExchangeManager & exchangeMgr, const chip::SessionHandle & sessionHandle)
{
    // Cast the context back to the class instance and call the actual member
    reinterpret_cast<BartonMatterImplementation*>(context)->OnSessionEstablished(sessionHandle);
}

void BartonMatterImplementation::OnSessionFailureStatic(void * context, const chip::ScopedNodeId & peerId, CHIP_ERROR error)
{
    // Cast the context back to the class instance and call the actual member
    reinterpret_cast<BartonMatterImplementation*>(context)->OnSessionFailure(peerId, error);
}

void BartonMatterImplementation::OnSessionEstablished(const chip::SessionHandle & sessionHandle)
{
        using namespace chip;

        chip::Messaging::ExchangeManager * exchangeMgr = &chip::Server::Server::GetInstance().GetExchangeManager();
        ChipLogProgress(AppServer, "Session established with Node: 0x" ChipLogFormatX64 " on Fabric %u",
                    ChipLogValueX64(sessionHandle->GetPeer().GetNodeId()),
                    sessionHandle->GetFabricIndex());

        // Get fabric info to retrieve local node ID
        chip::FabricIndex fabricIndex = sessionHandle->GetFabricIndex();
        const chip::FabricInfo * fabricInfo = chip::Server::GetInstance().GetFabricTable().FindFabricWithIndex(fabricIndex);
        if (fabricInfo == nullptr)
        {
            ChipLogError(AppServer, "Failed to find fabric info for fabric index %u", fabricIndex);
            return;
        }

        chip::NodeId localNodeId = fabricInfo->GetNodeId();
        chip::NodeId targetNodeId = sessionHandle->GetPeer().GetNodeId();

        ChipLogProgress(AppServer, "Local Node ID: 0x" ChipLogFormatX64, ChipLogValueX64(localNodeId));
        ChipLogProgress(AppServer, "Target Node ID: 0x" ChipLogFormatX64, ChipLogValueX64(targetNodeId));

        // Step 1: Read vendor and product IDs from the commissioned device
        // Try to retrieve from stored device information or use defaults
        uint16_t targetVendorId = 0xFFF1;  // Default vendor ID
        uint16_t targetProductId = 0x8000; // Default product ID

        if (!GetDeviceVendorProductIds(mClientDeviceUuid, targetVendorId, targetProductId))
        {
            ChipLogError(AppServer, "Failed to retrieve vendor/product IDs for device %s, using defaults",
                        mClientDeviceUuid.c_str());
        }

        ChipLogProgress(AppServer, "Target vendor ID: 0x%04X, product ID: 0x%04X",
                       targetVendorId, targetProductId);

        // The ACL entry has already been created in AddACLEntryForClient()
        // Now write bindings to the client device to inform it of accessible endpoints

        ChipLogProgress(AppServer, "Session established successfully");
        ChipLogProgress(AppServer, "ACL entry already configured for client node 0x" ChipLogFormatX64,
                       ChipLogValueX64(targetNodeId));

        /**
         * Get list of Barton endpoints to create bindings for
         * Barton has these endpoints per barton.zap:
         * - Endpoint 1: Video player (MA-videoplayer)
         * - Endpoint 2: Speaker (MA-speaker)
         * - Endpoint 3: Content application (MA-contentapplication)
         *
         * Following ManageClientAccess pattern: create bindings for all accessible endpoints
         */
        std::vector<chip::EndpointId> endpoints = { 1, 2, 3 };

        ChipLogProgress(AppServer, "Writing bindings to client for %zu Barton endpoints", endpoints.size());

        // WriteClientBindings uses callbacks for success/failure notification
        WriteClientBindings(*exchangeMgr, sessionHandle, localNodeId, endpoints);
}
void BartonMatterImplementation::OnSessionFailure(const chip::ScopedNodeId & peerId, CHIP_ERROR error)
{
    ChipLogError(AppServer, "CASESession establishment failed for Node 0x" ChipLogFormatX64
                           " on Fabric %u. Error: %s",
                 ChipLogValueX64(peerId.GetNodeId()),
                 peerId.GetFabricIndex(),
                 chip::ErrorStr(error));
}

        /**
         * @brief Retrieve vendor and product IDs for a commissioned device
         *
         * This method attempts to retrieve the vendor and product IDs from the device.
         * Currently uses default values, but should be extended to:
         * 1. Query Barton device metadata if available
         * 2. Read from Matter Basic Information cluster (async operation)
         * 3. Cache results for subsequent calls
         *
         * @param deviceUuid The device UUID (Matter node ID in hex)
         * @param vendorId Output parameter for vendor ID
         * @param productId Output parameter for product ID
         * @return true if IDs were retrieved successfully, false if using defaults
         */
        bool BartonMatterImplementation::GetDeviceVendorProductIds(const std::string& deviceUuid,
                                                                    uint16_t& vendorId,
                                                                    uint16_t& productId)
        {
            if (deviceUuid.empty())
            {
                LOGERR("GetDeviceVendorProductIds: Empty deviceUuid");
                return false;
            }

            // Check if we have cached values
            if (mClientVendorId != 0 && mClientProductId != 0 && mClientDeviceUuid == deviceUuid)
            {
                vendorId = mClientVendorId;
                productId = mClientProductId;
                LOGINFO("Using cached vendor/product IDs: 0x%04X/0x%04X for device %s",
                       vendorId, productId, deviceUuid.c_str());
                return true;
            }

            // Try to get device from Barton and read metadata
            if (bartonClient)
            {
                g_autolist(BCoreDevice) deviceObjects = b_core_client_get_devices(bartonClient);

                for (GList *iter = deviceObjects; iter != NULL; iter = iter->next)
                {
                    BCoreDevice *device = B_CORE_DEVICE(iter->data);
                    g_autofree gchar *uuid = NULL;

                    g_object_get(device, B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID], &uuid, NULL);

                    if (uuid && deviceUuid == std::string(uuid))
                    {
                        // Found the device - try to get metadata
                        // Barton might store Matter-specific metadata like "matter.vendorId" and "matter.productId"
                        // TODO: Query device metadata for vendor/product IDs if available

                        LOGINFO("Found device %s in Barton, but metadata query not yet implemented", uuid);
                        break;
                    }
                }
            }

            // For now, use reasonable default values for Matter casting clients
            // In a complete implementation, you would:
            // 1. Use chip::Controller::ReadAttribute to read Basic Information cluster
            //    - Cluster ID: 0x0028
            //    - Attribute VendorID: 0x0002
            //    - Attribute ProductID: 0x0004
            // 2. Handle the async response in a callback
            // 3. Cache the results

            vendorId = 0xFFF1;  // CSA test vendor ID
            productId = 0x8001; // Generic video player product ID

            LOGWARN("Using default vendor/product IDs: 0x%04X/0x%04X for device %s",
                   vendorId, productId, deviceUuid.c_str());
            LOGWARN("TODO: Implement Basic Information cluster read for actual values");

            return false; // Returning false indicates we're using defaults
        }

        /**
         * @brief Convert a Matter device UUID (hex string) to a numeric node ID
         *
         * Matter node IDs are 64-bit unsigned integers, but Barton stores them as hex strings.
         * This function converts from the string representation to the numeric value needed
         * by the Matter SDK APIs.
         *
         * @param deviceUuid The device UUID as a hex string (e.g., "90034FD9068DFF14")
         * @param nodeId Output parameter that receives the numeric node ID
         * @return true if conversion succeeded, false if the string was invalid
         */
        bool BartonMatterImplementation::GetNodeIdFromDeviceUuid(const std::string& deviceUuid, uint64_t& nodeId)
        {
            if (deviceUuid.empty())
            {
                LOGERR("GetNodeIdFromDeviceUuid: Empty deviceUuid");
                return false;
            }

            // Convert hex string to uint64_t
            // deviceUuid is already in hex format (e.g., "90034FD9068DFF14")
            char* endPtr = nullptr;
            nodeId = strtoull(deviceUuid.c_str(), &endPtr, 16);

            if (endPtr == deviceUuid.c_str() || *endPtr != '\0')
            {
                LOGERR("GetNodeIdFromDeviceUuid: Failed to parse deviceUuid '%s' as hex", deviceUuid.c_str());
                return false;
            }

            LOGINFO("GetNodeIdFromDeviceUuid: Converted '%s' to 0x%016llx (%llu)",
                    deviceUuid.c_str(), (unsigned long long)nodeId, (unsigned long long)nodeId);
            return true;
        }

        Core::hresult BartonMatterImplementation::ListDevices(std::string& deviceList /* @out */)
        {
            LOGINFO("Listing connected devices...");

            if (!bartonClient) {
                LOGERR("Barton client not initialized. Call InitializeCommissioner first.");
                deviceList = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            std::vector<std::string> deviceUuids;

            // Get all connected devices using Barton's API
            g_autolist(BCoreDevice) deviceObjects = b_core_client_get_devices(bartonClient);

            if (!deviceObjects) {
                LOGWARN("No devices found - device list is empty");
                deviceList = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            // Count devices and populate list
            for (GList *devicesIter = deviceObjects; devicesIter != NULL; devicesIter = devicesIter->next) {
                BCoreDevice *device = B_CORE_DEVICE(devicesIter->data);

                g_autofree gchar *deviceId = NULL;
                g_object_get(device,
                            B_CORE_DEVICE_PROPERTY_NAMES[B_CORE_DEVICE_PROP_UUID],
                            &deviceId,
                            NULL);

                if (deviceId != NULL) {
                    deviceUuids.push_back(std::string(deviceId));
                    LOGINFO("Found device: %s", deviceId);
                }
            }

            //if no valid device IDs were found
            if (deviceUuids.empty()) {
                LOGWARN("No valid device IDs found in device list");
                deviceList = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            // Convert to JSON
            deviceList = "[";
            for (size_t i = 0; i < deviceUuids.size(); ++i) {
                deviceList += "\"" + deviceUuids[i] + "\"";
                if (i < deviceUuids.size() - 1) {
                    deviceList += ",";
                }
            }
            deviceList += "]";

            LOGINFO("Total devices found: %zu", deviceUuids.size());
            return Core::ERROR_NONE;
        }

        Core::hresult BartonMatterImplementation::GetCommissionedDeviceInfo(std::string& deviceInfo /* @out */)
        {
            std::lock_guard<std::mutex> lock(devicesCacheMtx);

            // Always rescan devicedb so live resource values (isOn, currentLevel, etc.) are fresh
            ScanDeviceDatabase();
            devicesCacheInitialized = true;

            if (commissionedDevicesCache.empty()) {
                LOGWARN("No commissioned devices found");
                deviceInfo = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            // Build JSON array response with all devices.
            // Each entry contains: nodeId, deviceClass, deviceDriver, and a
            // "resources" object holding every id->value pair found in
            // deviceEndpoints.1.resources (e.g. label, isOn, currentLevel, colorXY).
            deviceInfo = "[";
            bool first = true;

            for (const auto& kv : commissionedDevicesCache) {
                const std::string& nodeId  = kv.first;
                const DeviceInfo&  info    = kv.second;

                if (!first) deviceInfo += ",";
                first = false;

                deviceInfo += "{";
                deviceInfo += "\"nodeId\":\""      + nodeId           + "\"";
                deviceInfo += ",\"deviceClass\":\"" + info.deviceClass  + "\"";
                deviceInfo += ",\"deviceDriver\":\""+ info.deviceDriver + "\"";

                // Emit resources object
                deviceInfo += ",\"resources\":{";
                bool firstRes = true;
                for (const auto& res : info.resources) {
                    if (!firstRes) deviceInfo += ",";
                    firstRes = false;
                    deviceInfo += "\"" + res.first + "\":\"" + res.second + "\"";
                }
                deviceInfo += "}";

                deviceInfo += "}";
            }

            deviceInfo += "]";

            LOGINFO("Returning %zu commissioned devices", commissionedDevicesCache.size());
            return Core::ERROR_NONE;
        }

        Core::hresult BartonMatterImplementation::RemoveDevice(const std::string deviceUuid /* @in */)
        {
            LOGINFO("RemoveDevice called for device: %s", deviceUuid.c_str());

            if (!bartonClient) {
                LOGERR("Barton client not initialized");
                return Core::ERROR_UNAVAILABLE;
            }

            if (deviceUuid.empty()) {
                LOGERR("Invalid device UUID: cannot be empty");
                return Core::ERROR_INVALID_INPUT_LENGTH;
            }

            gboolean result = b_core_client_remove_device(bartonClient, deviceUuid.c_str());

            if (result) {
                LOGINFO("Successfully removed device %s (devicedb file and all data deleted)", deviceUuid.c_str());

                // Clear cache and force rescan on next GetCommissionedDeviceInfo call
                // This ensures cache stays in sync with filesystem
                {
                    std::lock_guard<std::mutex> lock(devicesCacheMtx);
                    commissionedDevicesCache.clear();
                    devicesCacheInitialized = false;
                    LOGINFO("Cleared device cache and reset initialization flag to force rescan");
                }

                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to remove device %s", deviceUuid.c_str());
                return Core::ERROR_GENERAL;
            }
        }

        /**
         * @brief Extract a top-level string value from JSON content.
         *
         * Handles direct string fields such as "deviceClass", "deviceDriver", "uuid"
         * whose JSON representation is:  "key":  "value"
         *
         * @param content  Raw JSON text
         * @param key      The field name to look up
         * @return         The string value, or empty string if not found
         */
        static std::string ExtractTopLevelString(const std::string& content, const std::string& key)
        {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = content.find(searchKey);
            if (pos == std::string::npos) return "";

            size_t colonPos = content.find(':', pos + searchKey.size());
            if (colonPos == std::string::npos) return "";

            size_t valueStart = content.find('"', colonPos + 1);
            if (valueStart == std::string::npos) return "";
            valueStart++; // step past the opening quote

            size_t valueEnd = content.find('"', valueStart);
            if (valueEnd == std::string::npos) return "";

            return content.substr(valueStart, valueEnd - valueStart);
        }

        /**
         * @brief Extract all resource id->value pairs from a devicedb JSON file.
         *
         * Navigates to the path:
         *   deviceEndpoints -> "1" -> resources
         * and for every resource object found there reads the "value" field.
         *
         * The "value" field may be a quoted string ("tanuj", "true", "24939,24701")
         * or an unquoted scalar (254).  Both are returned as std::string.
         *
         * @param content  Raw JSON text of the devicedb file
         * @return         Map of resource id (e.g. "label", "isOn") -> value string
         */
        static std::map<std::string, std::string> ExtractAllResources(const std::string& content)
        {
            std::map<std::string, std::string> resources;

            // Anchor search inside "deviceEndpoints"
            size_t endpointsPos = content.find("\"deviceEndpoints\"");
            if (endpointsPos == std::string::npos) return resources;

            // Find "resources" inside the endpoints block
            size_t resourcesPos = content.find("\"resources\"", endpointsPos);
            if (resourcesPos == std::string::npos) return resources;

            // Find the opening brace of the resources object
            size_t resourcesOpen = content.find('{', resourcesPos + 11); // 11 = len("\"resources\"")
            if (resourcesOpen == std::string::npos) return resources;

            // Find the matching closing brace of the resources object
            size_t resourcesClose = resourcesOpen + 1;
            {
                int depth = 1;
                while (resourcesClose < content.size() && depth > 0) {
                    if (content[resourcesClose] == '{') ++depth;
                    else if (content[resourcesClose] == '}') --depth;
                    if (depth > 0) ++resourcesClose;
                }
            }

            // Iterate over each child of the resources object.
            // Each child looks like:  "resourceName": { ... "value": "...", ... }
            size_t pos = resourcesOpen + 1;
            while (pos < resourcesClose) {
                // Find the next quoted key (resource name)
                size_t keyStart = content.find('"', pos);
                if (keyStart == std::string::npos || keyStart >= resourcesClose) break;
                size_t keyEnd = content.find('"', keyStart + 1);
                if (keyEnd == std::string::npos || keyEnd >= resourcesClose) break;

                std::string resourceName = content.substr(keyStart + 1, keyEnd - keyStart - 1);

                // Find the opening brace of this resource's object
                size_t objOpen = content.find('{', keyEnd + 1);
                if (objOpen == std::string::npos || objOpen >= resourcesClose) break;

                // Find the matching closing brace of this resource's object
                size_t objClose = objOpen + 1;
                {
                    int depth = 1;
                    while (objClose < content.size() && depth > 0) {
                        if (content[objClose] == '{') ++depth;
                        else if (content[objClose] == '}') --depth;
                        if (depth > 0) ++objClose;
                    }
                }

                // Now parse the "value" field within [objOpen, objClose]
                size_t valueKeyPos = content.find("\"value\"", objOpen);
                if (valueKeyPos != std::string::npos && valueKeyPos < objClose) {
                    size_t colonPos = content.find(':', valueKeyPos + 7);
                    if (colonPos != std::string::npos && colonPos < objClose) {
                        // Skip whitespace after colon
                        size_t vs = colonPos + 1;
                        while (vs < objClose &&
                               (content[vs] == ' ' || content[vs] == '\t' ||
                                content[vs] == '\n' || content[vs] == '\r')) {
                            ++vs;
                        }
                        if (vs < objClose) {
                            if (content[vs] == '"') {
                                // Quoted string value
                                ++vs; // skip opening quote
                                size_t ve = content.find('"', vs);
                                if (ve != std::string::npos && ve <= objClose) {
                                    resources[resourceName] = content.substr(vs, ve - vs);
                                }
                            } else {
                                // Unquoted scalar (number / boolean)
                                size_t ve = vs;
                                while (ve < objClose &&
                                       content[ve] != ',' && content[ve] != '}' &&
                                       content[ve] != '\n' && content[ve] != '\r') {
                                    ++ve;
                                }
                                // Trim trailing whitespace
                                while (ve > vs &&
                                       (content[ve - 1] == ' ' || content[ve - 1] == '\t')) {
                                    --ve;
                                }
                                resources[resourceName] = content.substr(vs, ve - vs);
                            }
                        }
                    }
                }

                pos = objClose + 1;
            }

            return resources;
        }

        void BartonMatterImplementation::ScanDeviceDatabase()
        {
            const std::string dbPath = "/opt/.brtn-ds/storage/devicedb";

            DIR* dir = opendir(dbPath.c_str());
            if (!dir) {
                LOGWARN("Could not open devicedb directory: %s", dbPath.c_str());
                return;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string filename(entry->d_name);

                // Skip ".", "..", backup files, and system files
                if (filename[0] == '.' ||
                    filename.find(".bak") != std::string::npos ||
                    filename.find("system") != std::string::npos) {
                    continue;
                }

                // This is a device file (node ID = filename)
                std::string nodeId = filename;
                std::string filePath = dbPath + "/" + filename;

                // Read the device file content
                std::ifstream file(filePath, std::ios::binary);
                if (!file.is_open()) {
                    LOGWARN("Could not open device file: %s", filePath.c_str());
                    continue;
                }

                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();

                // Extract top-level device metadata
                DeviceInfo info;
                info.deviceClass  = ExtractTopLevelString(content, "deviceClass");
                info.deviceDriver = ExtractTopLevelString(content, "deviceDriver");

                // Extract all resource values from deviceEndpoints.1.resources
                info.resources = ExtractAllResources(content);

                // Only cache entries that have at least a device class
                if (!info.deviceClass.empty()) {
                    commissionedDevicesCache[nodeId] = info;

                    std::string labelVal = info.resources.count("label") ? info.resources.at("label") : "(none)";
                    LOGINFO("Found device in DB: nodeId=%s, deviceClass='%s', deviceDriver='%s', label='%s', resources=%zu",
                           nodeId.c_str(), info.deviceClass.c_str(), info.deviceDriver.c_str(),
                           labelVal.c_str(), info.resources.size());
                }
            }

            closedir(dir);
            LOGINFO("Device database scan complete. Found %zu devices", commissionedDevicesCache.size());
        }

        std::string BartonMatterImplementation::ExtractModelFromDeviceFile(const std::string& filePath)
        {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                LOGWARN("Could not open device file: %s", filePath.c_str());
                return "";
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();

            // Return deviceClass as a proxy for model (actual model lives in resources.label)
            return ExtractTopLevelString(content, "deviceClass");
        }

        void BartonMatterImplementation::UpdateDeviceCache(const std::string& nodeId, const std::string& deviceClass)
        {
            std::lock_guard<std::mutex> lock(devicesCacheMtx);

            // Seed the cache with the device class received at commission time.
            // Full resource values will be populated on the next ScanDeviceDatabase() call
            // (i.e. when GetCommissionedDeviceInfo() is invoked).
            DeviceInfo info;
            info.deviceClass  = deviceClass;
            info.deviceDriver = ""; // Not yet available — filled in by ScanDeviceDatabase
            // resources left empty — filled in by ScanDeviceDatabase

            commissionedDevicesCache[nodeId] = info;

            LOGINFO("Seeded device cache: nodeId=%s, deviceClass='%s' (total devices: %zu)",
                   nodeId.c_str(), deviceClass.c_str(), commissionedDevicesCache.size());
        }

        void BartonMatterImplementation::RemoveDeviceFromCache(const std::string& nodeId)
        {
            std::lock_guard<std::mutex> lock(devicesCacheMtx);

            auto it = commissionedDevicesCache.find(nodeId);
            if (it != commissionedDevicesCache.end()) {
                commissionedDevicesCache.erase(it);
                LOGINFO("Removed device from cache: nodeId=%s (remaining devices: %zu)",
                       nodeId.c_str(), commissionedDevicesCache.size());
            } else {
                LOGWARN("Device not found in cache: nodeId=%s", nodeId.c_str());
            }
        }

        /**
         * @brief Open a commissioning window on this device to allow Alexa or other controllers to commission it
         *
         * This method opens a Matter commissioning window on the local device and returns the
         * 11-digit manual setup code and QR code that controllers (like Alexa) can use to commission
         * the device. The commissioning window stays open for the specified timeout period.
         *
         * @param timeoutSeconds How long to keep the commissioning window open (0 = default timeout)
         * @param commissioningInfo Output JSON string containing manualCode and qrCode
         * @return Core::ERROR_NONE on success, error code otherwise
         */
        Core::hresult BartonMatterImplementation::OpenCommissioningWindow(
            const uint16_t timeoutSeconds /* @in */,
            std::string& commissioningInfo /* @out */)
        {
            LOGINFO("OpenCommissioningWindow called with timeout: %u seconds", timeoutSeconds);

            if (!bartonClient) {
                LOGERR("Barton client not initialized");
                commissioningInfo = "{}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Call Barton Core API to open commissioning window
            // Pass NULL or "0" for deviceId to open window on local device
            g_autoptr(BCoreCommissioningInfo) info =
                b_core_client_open_commissioning_window(bartonClient, "0", timeoutSeconds);

            if (!info) {
                LOGERR("Failed to open commissioning window");
                commissioningInfo = "{}";
                return Core::ERROR_GENERAL;
            }

            // Extract manual code (11-digit setup code) and QR code from the returned info
            g_autofree gchar *manualCode = NULL;
            g_autofree gchar *qrCode = NULL;

            g_object_get(info,
                        "manual-code", &manualCode,
                        "qr-code", &qrCode,
                        NULL);

            if (!manualCode || !qrCode) {
                LOGERR("Failed to retrieve commissioning codes from info object");
                commissioningInfo = "{}";
                return Core::ERROR_GENERAL;
            }

            // Build JSON response
            commissioningInfo = "{";
            commissioningInfo += "\"manualCode\":\"" + std::string(manualCode) + "\"";
            commissioningInfo += ",\"qrCode\":\"" + std::string(qrCode) + "\"";
            commissioningInfo += "}";

            LOGINFO("Commissioning window opened successfully");
            LOGINFO("  Manual Code (11-digit): %s", manualCode);
            LOGINFO("  QR Code: %s", qrCode);

            return Core::ERROR_NONE;
        }

        // ============================================================
        // Voice Command Processing System Implementation
        // ============================================================

        /**
         * @brief Main orchestrator for voice command processing
         * 
         * This method:
         * 1. Parses the natural language command
         * 2. Retrieves commissioned devices
         * 3. Matches command to a specific device
         * 4. Executes the appropriate Matter command
         */
        bool BartonMatterImplementation::ExecuteVoiceAction(const std::string& voiceCommand)
        {
            LOGINFO("Processing voice command: '%s'", voiceCommand.c_str());

            // Step 1: Parse the voice command
            VoiceCommand cmd = ParseVoiceCommand(voiceCommand);
            
            if (!cmd.isValid()) {
                LOGERR("Failed to parse voice command: '%s'", voiceCommand.c_str());
                return false;
            }

            LOGINFO("Parsed command - Action: %d, Device: '%s', Qualifier: '%s'",
                   static_cast<int>(cmd.action), cmd.deviceType.c_str(), cmd.deviceQualifier.c_str());

            // Step 2: Get list of commissioned devices
            std::string deviceInfo;
            Core::hresult result = GetCommissionedDeviceInfo(deviceInfo);
            
            if (result != Core::ERROR_NONE || deviceInfo == "[]") {
                LOGERR("No commissioned devices available");
                return false;
            }

            LOGINFO("Retrieved device info: %s", deviceInfo.c_str());

            // Step 3: Find matching device
            DeviceMatch match = FindMatchingDevice(cmd, deviceInfo);
            
            if (!match.isValid()) {
                LOGERR("No matching device found for: '%s %s'", 
                      cmd.deviceQualifier.c_str(), cmd.deviceType.c_str());
                return false;
            }

            LOGINFO("Found matching device - NodeId: %s, Model: %s, Confidence: %d%%",
                   match.nodeId.c_str(), match.model.c_str(), match.confidence);

            // Step 4: Map action to Matter resource
            std::string resourceType;
            std::string value;
            
            if (!MapActionToResource(cmd.action, cmd.levelValue, cmd.colorName, resourceType, value)) {
                LOGERR("Failed to map action to resource");
                return false;
            }

            LOGINFO("Mapped to resource - Type: %s, Value: %s", resourceType.c_str(), value.c_str());

            // Step 5: Execute the command
            Core::hresult writeResult = WriteResource(match.nodeId, resourceType, value);
            
            if (writeResult != Core::ERROR_NONE) {
                LOGERR("Failed to write resource to device %s", match.nodeId.c_str());
                return false;
            }

            LOGINFO("Successfully executed voice command on device %s", match.nodeId.c_str());
            return true;
        }

        /**
         * @brief Parse natural language command into structured format
         * 
         * Supports patterns:
         * - "turn on the light"
         * - "turn off bedroom light"
         * - "switch on kitchen plug"
         * - "turn the living room lamp off"
         * - "dim the light"
         * - "brighten bedroom light"
         */
        BartonMatterImplementation::VoiceCommand BartonMatterImplementation::ParseVoiceCommand(const std::string& text) const
        {
            VoiceCommand cmd;
            std::string normalized = NormalizeText(text);

            LOGINFO("Parsing normalized command: '%s'", normalized.c_str());

            // Parse action keywords
            if (normalized.find("turn on") != std::string::npos || 
                normalized.find("switch on") != std::string::npos ||
                normalized.find("power on") != std::string::npos ||
                normalized.find("enable") != std::string::npos) {
                cmd.action = VoiceCommand::Action::TURN_ON;
            }
            else if (normalized.find("turn off") != std::string::npos || 
                     normalized.find("switch off") != std::string::npos ||
                     normalized.find("power off") != std::string::npos ||
                     normalized.find("disable") != std::string::npos) {
                cmd.action = VoiceCommand::Action::TURN_OFF;
            }
            else if (normalized.find("toggle") != std::string::npos) {
                cmd.action = VoiceCommand::Action::TOGGLE;
            }
            else if (normalized.find("dim") != std::string::npos || 
                     normalized.find("lower") != std::string::npos ||
                     normalized.find("decrease") != std::string::npos) {
                cmd.action = VoiceCommand::Action::DIM;
            }
            else if (normalized.find("brighten") != std::string::npos || 
                     normalized.find("brighter") != std::string::npos ||
                     normalized.find("increase") != std::string::npos) {
                cmd.action = VoiceCommand::Action::BRIGHTEN;
            }
            else if (normalized.find("set brightness") != std::string::npos ||
                     normalized.find("brightness to") != std::string::npos ||
                     normalized.find("brightness at") != std::string::npos) {
                // "set brightness to 75%"  /  "set the bedroom light brightness to 50%"
                cmd.action = VoiceCommand::Action::SET_BRIGHTNESS;

                // Extract the numeric percentage that precedes a % character
                size_t pctPos = normalized.find('%');
                if (pctPos != std::string::npos) {
                    size_t numEnd = pctPos;
                    size_t numStart = numEnd;
                    while (numStart > 0 && std::isdigit(static_cast<unsigned char>(normalized[numStart - 1]))) {
                        --numStart;
                    }
                    if (numStart < numEnd) {
                        int pct = std::stoi(normalized.substr(numStart, numEnd - numStart));
                        cmd.levelValue = std::max(0, std::min(100, pct));
                    }
                }

                if (cmd.levelValue < 0) {
                    cmd.levelValue = 50; // Sensible default when no percentage is detected
                    LOGWARN("ParseVoiceCommand: No percentage found in brightness command, defaulting to 50%%");
                }
            }
            else if (normalized.find("set color") != std::string::npos ||
                     normalized.find("change color") != std::string::npos ||
                     normalized.find("color to") != std::string::npos ||
                     normalized.find("make it") != std::string::npos) {
                // "set the light to red"  /  "change color to blue"  /  "make it yellow"
                cmd.action = VoiceCommand::Action::SET_COLOR;

                // Match against the 7 supported CIE 1931 colour names
                const std::vector<std::string> supportedColors = {
                    "red", "green", "blue", "yellow", "purple", "cyan", "white"
                };
                for (const auto& color : supportedColors) {
                    if (normalized.find(color) != std::string::npos) {
                        cmd.colorName = color;
                        break;
                    }
                }

                if (cmd.colorName.empty()) {
                    LOGWARN("ParseVoiceCommand: SET_COLOR detected but no supported colour found in: '%s'",
                            normalized.c_str());
                }
            }

            // ---------------------------------------------------------------
            // Fallback colour detection: catches natural phrasing that the
            // explicit triggers above miss, e.g.:
            //   "change the bedroom light to red"
            //   "set bedroom light to red"
            //   "set kitchen light to blue"
            //   "change the bedroom light color to blue"  ← already caught above
            //
            // Strategy: if action is still UNKNOWN and the command contains
            //   - an intent verb (set / change / make / turn)
            //   - the word " to "
            //   - one of the 7 supported colour names
            // ... then treat it as SET_COLOR.
            //
            // NOTE: Simple typos in the colour name (e.g. "greeen", "bllue")
            // will NOT match because we use exact substring search. A fuzzy
            // matcher would be needed to handle arbitrary misspellings.
            // Typos in the device/location qualifier (e.g. "kitched" instead of
            // "kitchen") only affect device matching, not action detection.
            // ---------------------------------------------------------------
            if (cmd.action == VoiceCommand::Action::UNKNOWN) {
                const std::vector<std::string> colorIntentVerbs = {
                    "set", "change", "make", "turn"
                };
                const std::vector<std::string> colorNames = {
                    "red", "green", "blue", "yellow", "purple", "cyan", "white"
                };

                bool hasVerb = false;
                for (const auto& verb : colorIntentVerbs) {
                    // Match whole word only: preceded by start or space
                    size_t vpos = normalized.find(verb);
                    if (vpos != std::string::npos &&
                        (vpos == 0 || normalized[vpos - 1] == ' ')) {
                        hasVerb = true;
                        break;
                    }
                }

                bool hasTo = (normalized.find(" to ") != std::string::npos);

                if (hasVerb && hasTo) {
                    for (const auto& color : colorNames) {
                        if (normalized.find(color) != std::string::npos) {
                            cmd.action   = VoiceCommand::Action::SET_COLOR;
                            cmd.colorName = color;
                            LOGINFO("ParseVoiceCommand: Fallback colour detection matched '%s' in '%s'",
                                    color.c_str(), normalized.c_str());
                            break;
                        }
                    }
                }

                if (cmd.action == VoiceCommand::Action::UNKNOWN) {
                    LOGWARN("ParseVoiceCommand: Could not determine action from: '%s'", normalized.c_str());
                }
            }

            // Parse device type - check for common device types and synonyms
            const std::vector<std::pair<std::string, std::vector<std::string>>> deviceTypes = {
                {"light", {"light", "lamp", "bulb", "lighting"}},
                {"plug", {"plug", "outlet", "socket"}},
                {"switch", {"switch"}},
                {"fan", {"fan"}},
                {"thermostat", {"thermostat", "temperature"}},
                {"lock", {"lock", "door lock"}},
                {"blind", {"blind", "shade", "curtain"}},
                {"sensor", {"sensor", "detector"}}
            };

            for (const auto& deviceType : deviceTypes) {
                for (const auto& synonym : deviceType.second) {
                    if (normalized.find(synonym) != std::string::npos) {
                        cmd.deviceType = deviceType.first;
                        break;
                    }
                }
                if (!cmd.deviceType.empty()) break;
            }

            // Parse location/qualifier - common room/location names
            const std::vector<std::string> qualifiers = {
                "bedroom", "living room", "kitchen", "bathroom", "garage", 
                "hallway", "basement", "attic", "office", "dining room",
                "master", "guest", "kids", "front", "back", "main"
            };

            for (const auto& qualifier : qualifiers) {
                if (normalized.find(qualifier) != std::string::npos) {
                    cmd.deviceQualifier = qualifier;
                    break;
                }
            }

            return cmd;
        }

        /**
         * @brief Find best matching device from commissioned devices list
         * 
         * Matching strategy:
         * 1. Exact match on device type + qualifier
         * 2. Partial match on device type
         * 3. Fuzzy match on model name
         */
        BartonMatterImplementation::DeviceMatch BartonMatterImplementation::FindMatchingDevice(
            const VoiceCommand& command, const std::string& deviceInfo) const
        {
            DeviceMatch bestMatch;

            // Parse the JSON produced by GetCommissionedDeviceInfo().
            // Format (one entry):
            //   {"nodeId":"xxx","deviceClass":"light","deviceDriver":"matterLight",
            //    "resources":{"label":"tanuj","isOn":"true","currentLevel":"254",...}}
            size_t pos = 0;

            while ((pos = deviceInfo.find("\"nodeId\"", pos)) != std::string::npos) {
                // --- nodeId ---
                size_t nodeIdStart = deviceInfo.find(':', pos) + 1;
                while (nodeIdStart < deviceInfo.size() && (deviceInfo[nodeIdStart] == ' ' || deviceInfo[nodeIdStart] == '"')) ++nodeIdStart;
                size_t nodeIdEnd = deviceInfo.find('"', nodeIdStart);
                std::string nodeId = deviceInfo.substr(nodeIdStart, nodeIdEnd - nodeIdStart);

                // --- deviceClass ---
                std::string deviceClass;
                size_t classPos = deviceInfo.find("\"deviceClass\"", nodeIdEnd);
                if (classPos != std::string::npos) {
                    size_t classStart = deviceInfo.find(':', classPos) + 1;
                    while (classStart < deviceInfo.size() && (deviceInfo[classStart] == ' ' || deviceInfo[classStart] == '"')) ++classStart;
                    size_t classEnd = deviceInfo.find('"', classStart);
                    deviceClass = deviceInfo.substr(classStart, classEnd - classStart);
                }

                // --- resources.label (the user-visible name) ---
                std::string label;
                size_t resPos = deviceInfo.find("\"resources\"", nodeIdEnd);
                if (resPos != std::string::npos) {
                    size_t labelKeyPos = deviceInfo.find("\"label\"", resPos);
                    // Make sure we're still in this device's resources block
                    size_t nextBrace = deviceInfo.find('}', resPos);
                    if (labelKeyPos != std::string::npos && labelKeyPos < nextBrace) {
                        size_t labelStart = deviceInfo.find(':', labelKeyPos) + 1;
                        while (labelStart < deviceInfo.size() && (deviceInfo[labelStart] == ' ' || deviceInfo[labelStart] == '"')) ++labelStart;
                        size_t labelEnd = deviceInfo.find('"', labelStart);
                        if (labelEnd != std::string::npos && labelEnd < nextBrace + 20) {
                            label = deviceInfo.substr(labelStart, labelEnd - labelStart);
                        }
                    }
                }

                LOGINFO("Checking device - NodeId: %s, DeviceClass: %s, Label: %s",
                       nodeId.c_str(), deviceClass.c_str(), label.empty() ? "(none)" : label.c_str());

                // Determine which text to use for matching:
                // Prefer the user-set label (if it looks custom), fall back to deviceClass.
                std::string matchText = deviceClass;
                std::string normalizedLabel = NormalizeText(label);

                bool isCustomLabel = false;
                if (!label.empty()) {
                    const std::vector<std::string> qualifiers = {
                        "bedroom", "kitchen", "living room", "bathroom", "garage",
                        "hallway", "basement", "attic", "office", "dining room",
                        "master", "guest", "kids", "front", "back", "main",
                        "upstairs", "downstairs", "outside", "indoor", "outdoor"
                    };
                    for (const auto& qualifier : qualifiers) {
                        if (normalizedLabel.find(qualifier) != std::string::npos) {
                            isCustomLabel = true;
                            break;
                        }
                    }
                    if (!isCustomLabel && normalizedLabel != NormalizeText(deviceClass)) {
                        isCustomLabel = true;
                    }
                }

                if (isCustomLabel) {
                    matchText = label;
                    LOGINFO("Using custom label '%s' for matching", label.c_str());
                } else {
                    LOGINFO("Using deviceClass '%s' for matching", deviceClass.c_str());
                }

                int confidence = 0;
                std::string normalizedText = NormalizeText(matchText);

                // Score device type similarity
                int deviceTypeScore = CalculateSimilarity(command.deviceType, normalizedText);
                confidence += deviceTypeScore;

                // Score qualifier similarity (weighted less)
                if (!command.deviceQualifier.empty()) {
                    int qualifierScore = CalculateSimilarity(command.deviceQualifier, normalizedText);
                    confidence += qualifierScore / 2;
                }

                // Bonus for exact substring match on device type
                if (normalizedText.find(command.deviceType) != std::string::npos) {
                    confidence += 30;
                }

                LOGINFO("Device '%s' scored %d%% confidence", matchText.c_str(), confidence);

                if (confidence > bestMatch.confidence) {
                    bestMatch.nodeId      = nodeId;
                    bestMatch.model       = matchText;
                    bestMatch.confidence  = confidence;
                }

                pos = nodeIdEnd;
            }

            if (bestMatch.confidence < 30) {
                LOGWARN("Best match confidence too low: %d%%", bestMatch.confidence);
                bestMatch = DeviceMatch();
            }

            return bestMatch;
        }

        /**
         * @brief Map voice action to Matter resource type and value
         *
         * Matter resource mappings:
         * - OnOff cluster:    resourceType = "isOn",         value = "true" / "false"
         * - Level cluster:    resourceType = "level",        value = 0-254 (legacy)
         * - Brightness:       resourceType = "currentLevel", value = 0-254  (Matter Level Control)
         * - Color (CIE 1931): resourceType = "colorXY",      value = "x,y"
         */
        bool BartonMatterImplementation::MapActionToResource(
            VoiceCommand::Action action, int levelValue,
            const std::string& colorName,
            std::string& resourceType, std::string& value) const
        {
            switch (action) {
                case VoiceCommand::Action::TURN_ON:
                    resourceType = "isOn";
                    value = "true";
                    return true;

                case VoiceCommand::Action::TURN_OFF:
                    resourceType = "isOn";
                    value = "false";
                    return true;

                case VoiceCommand::Action::TOGGLE:
                    // Toggle requires reading current state first—default to ON for now
                    resourceType = "isOn";
                    value = "true";
                    LOGWARN("Toggle not fully implemented, defaulting to ON");
                    return true;

                case VoiceCommand::Action::DIM:
                    resourceType = "level";
                    value = "64"; // ~25% brightness (Matter level 0-254)
                    return true;

                case VoiceCommand::Action::BRIGHTEN:
                    resourceType = "level";
                    value = "254"; // 100% brightness
                    return true;

                case VoiceCommand::Action::SET_LEVEL:
                    if (levelValue >= 0 && levelValue <= 100) {
                        resourceType = "level";
                        value = std::to_string((levelValue * 254) / 100);
                        return true;
                    }
                    LOGERR("Invalid level value: %d", levelValue);
                    return false;

                // -----------------------------------------------------------------
                // New: SET_BRIGHTNESS — maps to Matter Level Control cluster
                //   resourceId : "currentLevel"
                //   value range: 0-254 (Matter scale)
                // -----------------------------------------------------------------
                case VoiceCommand::Action::SET_BRIGHTNESS: {
                    if (levelValue < 0 || levelValue > 100) {
                        LOGERR("SET_BRIGHTNESS: levelValue %d out of range [0,100]", levelValue);
                        return false;
                    }
                    resourceType = "currentLevel";
                    value = std::to_string(PercentageToMatterLevel(levelValue));
                    LOGINFO("SET_BRIGHTNESS: %d%% → Matter level %s", levelValue, value.c_str());
                    return true;
                }

                // -----------------------------------------------------------------
                // New: SET_COLOR — maps to Matter Color Control cluster (CIE 1931 XY)
                //   resourceId : "colorXY"
                //   value      : "x,y" (unsigned 16-bit CIE integers)
                // Supported: red, green, blue, yellow, purple, cyan, white
                // -----------------------------------------------------------------
                case VoiceCommand::Action::SET_COLOR: {
                    if (colorName.empty()) {
                        LOGERR("SET_COLOR: No colour name provided");
                        return false;
                    }
                    std::string xy = GetColorXY(colorName);
                    if (xy.empty()) {
                        LOGERR("SET_COLOR: Unsupported colour '%s'. "
                               "Supported: red, green, blue, yellow, purple, cyan, white",
                               colorName.c_str());
                        return false;
                    }
                    resourceType = "colorXY";
                    value = xy;
                    LOGINFO("SET_COLOR: '%s' → XY = %s", colorName.c_str(), value.c_str());
                    return true;
                }

                default:
                    LOGERR("Unknown action type");
                    return false;
            }
        }

        /**
         * @brief Convert a brightness percentage to the Matter Level Control scale.
         *
         * Matter Level Control attribute currentLevel uses an unsigned 8-bit value:
         *   0   = off / minimum
         *   254 = maximum (0xFF is reserved)
         *
         * Formula: matterLevel = round(percentage * 254 / 100)
         *
         * Examples:
         *   0%   → 0
         *   50%  → 127
         *   100% → 254
         *
         * @param percentage Value in [0, 100]. Clamped to this range if out of bounds.
         * @return Matter level in [0, 254]
         */
        int BartonMatterImplementation::PercentageToMatterLevel(int percentage)
        {
            // Clamp input to [0, 100] for safety
            int pct = std::max(0, std::min(100, percentage));
            // Use rounding: add 50 before integer division to get nearest integer
            return (pct * 254 + 50) / 100;
        }

        /**
         * @brief Return the CIE 1931 XY colour coordinates for a predefined colour name.
         *
         * Values are 16-bit unsigned integers as specified by the Matter Color Control
         * cluster (colorX / colorY attributes, each in range 0-65279). The string format
         * is "x,y" matching the "colorXY" resource type accepted by WriteResource().
         *
         * Colour table (7 entries):
         * | Name   | x     | y     | Notes                          |
         * |--------|-------|-------|--------------------------------|
         * | red    | 45712 | 18133 | sRGB red primary               |
         * | green  | 17242 | 43690 | sRGB green primary             |
         * | blue   |  6553 |  6553 | sRGB blue primary              |
         * | yellow | 38228 | 45000 | warm yellow                    |
         * | purple | 40000 | 20000 | violet-purple                  |
         * | cyan   | 15000 | 40000 | blue-green cyan                |
         * | white  | 24931 | 24703 | D65 white point (6500 K)       |
         *
         * @param colorName Lowercase colour name (e.g. "red", "blue").
         * @return "x,y" string, or empty string if the colour name is not recognised.
         */
        std::string BartonMatterImplementation::GetColorXY(const std::string& colorName)
        {
            // Static lookup table — avoids rebuilding on every call
            static const std::unordered_map<std::string, std::string> kColorXYTable = {
                { "red",    "45712,18133" },
                { "green",  "17242,43690" },
                { "blue",   "6553,6553"   },
                { "yellow", "38228,45000" },
                { "purple", "40000,20000" },
                { "cyan",   "15000,40000" },
                { "white",  "24931,24703" },
            };

            auto it = kColorXYTable.find(colorName);
            if (it != kColorXYTable.end()) {
                return it->second;
            }

            // Unknown colour — return empty string; caller must handle gracefully
            return {};
        }

        /**
         * @brief Normalize text for matching (lowercase, trim)
         */
        std::string BartonMatterImplementation::NormalizeText(const std::string& text) const
        {
            std::string result = text;
            
            // Convert to lowercase
            std::transform(result.begin(), result.end(), result.begin(), 
                         [](unsigned char c) { return std::tolower(c); });
            
            // Remove leading/trailing whitespace
            size_t start = result.find_first_not_of(" \t\n\r");
            size_t end = result.find_last_not_of(" \t\n\r");
            
            if (start != std::string::npos && end != std::string::npos) {
                result = result.substr(start, end - start + 1);
            }
            
            // Remove common punctuation
            result.erase(std::remove_if(result.begin(), result.end(),
                                       [](char c) { return c == '.' || c == ',' || c == '!' || c == '?'; }),
                        result.end());
            
            return result;
        }

        /**
         * @brief Calculate simple similarity score between two strings
         * 
         * Uses basic substring matching and common character counting
         */
        int BartonMatterImplementation::CalculateSimilarity(const std::string& str1, const std::string& str2) const
        {
            if (str1.empty() || str2.empty()) {
                return 0;
            }
            
            std::string s1 = NormalizeText(str1);
            std::string s2 = NormalizeText(str2);
            
            // Exact match
            if (s1 == s2) {
                return 100;
            }
            
            // Substring match
            if (s2.find(s1) != std::string::npos || s1.find(s2) != std::string::npos) {
                return 80;
            }
            
            // Count matching characters in sequence
            int matches = 0;
            size_t minLen = std::min(s1.length(), s2.length());
            
            for (size_t i = 0; i < minLen; ++i) {
                if (s1[i] == s2[i]) {
                    matches++;
                }
            }
            
            // Calculate percentage
            int score = (matches * 100) / std::max(s1.length(), s2.length());
            
            return score;
        }

	Core::hresult BartonMatterImplementation::OnVoiceCommandReceived(const std::string& payload /* @in */)
	{
		if (payload.empty()) {
			LOGWARN("[BartonMatter Plugin] Received empty voice command payload");
			return Core::ERROR_INVALID_INPUT_LENGTH;
		}

		LOGWARN("[BartonMatter Plugin] Received smart home voice command: '%s'", payload.c_str());

		// Process the voice command and execute the action
		bool success = ExecuteVoiceAction(payload);

		if (success) {
			LOGINFO("[BartonMatter Plugin] Voice command executed successfully");
			return Core::ERROR_NONE;
		} else {
			LOGERR("[BartonMatter Plugin] Failed to execute voice command");
			return Core::ERROR_GENERAL;
		}
	}

    } // namespace Plugin
} // namespace WPEFramework

// C to C++ Glue for GLib Network Credentials Provider
//
// - Barton Core library expects a GObject-based interface (pure C)
// - Thunder plugin is written in C++
// - This glue bridges the gap by implementing the required C interface
//   while allowing the C++ plugin to store/manage credentials
// - Data flows: Thunder API (C++) -> Global vars -> GLib interface (C) -> Barton Core
//
extern "C" {

struct _BReferenceNetworkCredentialsProvider
{
    GObject parent_instance;
};

static void
b_reference_network_credentials_provider_interface_init(BCoreNetworkCredentialsProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE(BReferenceNetworkCredentialsProvider,
                        b_reference_network_credentials_provider,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(B_CORE_NETWORK_CREDENTIALS_PROVIDER_TYPE,
                                              b_reference_network_credentials_provider_interface_init))

/*
 * Implementation of BCoreNetworkCredentialsProvider get_wifi_network_credentials
 */
static BCoreWifiNetworkCredentials *
b_reference_network_credentials_provider_get_wifi_network_credentials(
    BCoreNetworkCredentialsProvider *self,
    GError **error)
{
    g_return_val_if_fail(B_REFERENCE_IS_NETWORK_CREDENTIALS_PROVIDER(self), NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    g_autoptr(BCoreWifiNetworkCredentials) wifiCredentials = NULL;

    wifiCredentials = b_core_wifi_network_credentials_new();

    std::lock_guard<std::mutex> lock(networkCredsMtx);
    if (network_ssid != NULL && network_psk != NULL)
    {
        g_object_set(wifiCredentials,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_SSID],
                     network_ssid,
                     B_CORE_WIFI_NETWORK_CREDENTIALS_PROPERTY_NAMES
                         [B_CORE_WIFI_NETWORK_CREDENTIALS_PROP_PSK],
                     network_psk,
                     NULL);
    }

    return g_steal_pointer(&wifiCredentials);
}

static void
b_reference_network_credentials_provider_interface_init(BCoreNetworkCredentialsProviderInterface *iface)
{
    iface->get_wifi_network_credentials =
        b_reference_network_credentials_provider_get_wifi_network_credentials;
}

static void b_reference_network_credentials_provider_init(BReferenceNetworkCredentialsProvider *self)
{
    // No instance initialization needed
}

static void
b_reference_network_credentials_provider_class_init(BReferenceNetworkCredentialsProviderClass *klass)
{
    // No class initialization needed
}

void b_reference_network_credentials_provider_set_wifi_network_credentials(const gchar *ssid, const gchar *password)
{
    g_return_if_fail(ssid != NULL);
    g_return_if_fail(password != NULL);

    std::lock_guard<std::mutex> lock(networkCredsMtx);
    g_free(network_ssid);
    g_free(network_psk);

    network_ssid = g_strdup(ssid);
    network_psk = g_strdup(password);
}

BReferenceNetworkCredentialsProvider *b_reference_network_credentials_provider_new(void)
{
    return B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER(
        g_object_new(B_REFERENCE_NETWORK_CREDENTIALS_PROVIDER_TYPE, NULL));
}

} // extern "C"
