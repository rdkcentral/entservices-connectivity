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

            g_autoptr(GError) err = NULL;
            if(!b_core_client_write_resource(bartonClient, fullUri.c_str(), value.c_str()))
            {
                LOGERR("Write resource failed: %s", err->message);
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
                LOGWARN("Using default wifi credentials");
                b_reference_network_credentials_provider_set_wifi_network_credentials("MySSID", "MyPassword");
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

            // Scan devicedb on first call to populate cache
            if (!devicesCacheInitialized) {
                ScanDeviceDatabase();
                devicesCacheInitialized = true;
            }

            if (commissionedDevicesCache.empty()) {
                LOGWARN("No commissioned devices found");
                deviceInfo = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            // Build JSON array response with all devices
            // Include both model and label (label may be empty if not set)
            deviceInfo = "[";
            bool first = true;

            for (const auto& device : commissionedDevicesCache) {
                if (!first) {
                    deviceInfo += ",";
                }
                first = false;

                deviceInfo += "{";
                deviceInfo += "\"nodeId\":\"" + device.first + "\"";
                deviceInfo += ",\"model\":\"" + device.second.model + "\"";
                deviceInfo += ",\"label\":\"" + device.second.label + "\"";
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

                // This is a device file (node ID)
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

                // Extract both model and label
                DeviceInfo info;
                info.model = ExtractFieldValue(content, "model");
                info.label = ExtractFieldValue(content, "label");

                if (!info.model.empty()) {
                    commissionedDevicesCache[nodeId] = info;
                    LOGINFO("Found device in DB: nodeId=%s, model='%s', label='%s'",
                           nodeId.c_str(), info.model.c_str(),
                           info.label.empty() ? "(none)" : info.label.c_str());
                }
            }

            closedir(dir);
            LOGINFO("Device database scan complete. Found %zu devices", commissionedDevicesCache.size());
        }

        /**
         * @brief Extract label field value from device file content
         */
        std::string ExtractFieldValue(const std::string& content, const std::string& fieldName)
        {
            size_t fieldPos = content.find("\"" + fieldName + "\"");
            if (fieldPos == std::string::npos) {
                return "";
            }
            
            size_t valuePos = content.find("\"value\"", fieldPos);
            if (valuePos == std::string::npos || (valuePos - fieldPos) > 300) {
                return "";
            }
            
            size_t valueStart = content.find('"', valuePos + 7);
            if (valueStart == std::string::npos) {
                return "";
            }
            valueStart++;
            
            size_t valueEnd = content.find('"', valueStart);
            if (valueEnd == std::string::npos) {
                return "";
            }
            
            return content.substr(valueStart, valueEnd - valueStart);
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

            std::string modelName = ExtractFieldValue(content, "model");
            return modelName;
        }

        void BartonMatterImplementation::UpdateDeviceCache(const std::string& nodeId, const std::string& modelName)
        {
            std::lock_guard<std::mutex> lock(devicesCacheMtx);

            // Update or add device to cache (prevents duplicates)
            DeviceInfo info;
            info.model = modelName;
            info.label = ""; // Label will be populated on next database scan or when user sets it
            
            commissionedDevicesCache[nodeId] = info;

            LOGINFO("Updated device cache: nodeId=%s, model='%s' (total devices: %zu)",
                   nodeId.c_str(), modelName.c_str(), commissionedDevicesCache.size());
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
            
            if (!MapActionToResource(cmd.action, cmd.levelValue, resourceType, value)) {
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
            
            // Parse JSON manually (simple parsing for structure: [{"nodeId":"xxx","model":"yyy"},...]
            size_t pos = 0;
            
            while ((pos = deviceInfo.find("\"nodeId\"", pos)) != std::string::npos) {
                // Extract nodeId
                size_t nodeIdStart = deviceInfo.find(":", pos) + 2; // Skip :" 
                size_t nodeIdEnd = deviceInfo.find("\"", nodeIdStart);
                std::string nodeId = deviceInfo.substr(nodeIdStart, nodeIdEnd - nodeIdStart);
                
                // Extract model
                size_t modelPos = deviceInfo.find("\"model\"", nodeIdEnd);
                if (modelPos == std::string::npos) break;
                
                size_t modelStart = deviceInfo.find(":", modelPos) + 2; // Skip :"
                size_t modelEnd = deviceInfo.find("\"", modelStart);
                std::string model = deviceInfo.substr(modelStart, modelEnd - modelStart);
                
                // Extract label (may be empty)
                std::string label;
                size_t labelPos = deviceInfo.find("\"label\"", modelEnd);
                if (labelPos != std::string::npos && labelPos < deviceInfo.find("}", modelEnd)) {
                    size_t labelStart = deviceInfo.find(":", labelPos) + 2; // Skip :"
                    size_t labelEnd = deviceInfo.find("\"", labelStart);
                    label = deviceInfo.substr(labelStart, labelEnd - labelStart);
                }
                
                LOGINFO("Checking device - NodeId: %s, Model: %s, Label: %s", 
                       nodeId.c_str(), model.c_str(), label.empty() ? "(none)" : label.c_str());
                
                // Calculate match confidence
                // Determine which text to use for matching:
                // - Use label if it's custom (not default like "matter light", "matter plug")
                // - Use model if label is default/generic or empty
                
                std::string matchText = model;  // Default to model
                std::string normalizedLabel = NormalizeText(label);
                
                // Check if label is custom (not a default generic label)
                bool isCustomLabel = false;
                if (!label.empty()) {
                    // Default labels are typically: "matter light", "matter plug", etc.
                    // Custom labels have location/qualifiers: "bedroom plug", "kitchen light"
                    
                    // If label contains qualifier words, it's likely custom
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
                    
                    // Also check if it's NOT a default pattern like "matter [type]"
                    if (!isCustomLabel && normalizedLabel.find("matter") == std::string::npos) {
                        // If it doesn't contain "matter" and is different from model, likely custom
                        if (normalizedLabel != NormalizeText(model)) {
                            isCustomLabel = true;
                        }
                    }
                }
                
                if (isCustomLabel) {
                    matchText = label;
                    LOGINFO("Using custom label '%s' for matching", label.c_str());
                } else {
                    LOGINFO("Using model '%s' for matching (label is default/generic)", model.c_str());
                }
                
                int confidence = 0;
                std::string normalizedText = NormalizeText(matchText);
                
                // Check for device type match
                int deviceTypeScore = CalculateSimilarity(command.deviceType, normalizedText);
                confidence += deviceTypeScore;
                
                // Check for qualifier match
                if (!command.deviceQualifier.empty()) {
                    int qualifierScore = CalculateSimilarity(command.deviceQualifier, normalizedText);
                    confidence += qualifierScore / 2; // Weight qualifier less than device type
                }
                
                // Bonus for exact substring match
                if (normalizedText.find(command.deviceType) != std::string::npos) {
                    confidence += 30;
                }
                
                LOGINFO("Device '%s' scored %d%% confidence", matchText.c_str(), confidence);
                
                // Update best match if this is better
                if (confidence > bestMatch.confidence) {
                    bestMatch.nodeId = nodeId;
                    bestMatch.model = matchText;  // Store the text we matched against
                    bestMatch.confidence = confidence;
                }
                
                pos = modelEnd;
            }
            
            // Only return match if confidence is reasonable (>30%)
            if (bestMatch.confidence < 30) {
                LOGWARN("Best match confidence too low: %d%%", bestMatch.confidence);
                bestMatch = DeviceMatch(); // Reset to invalid
            }
            
            return bestMatch;
        }

        /**
         * @brief Map voice action to Matter resource type and value
         * 
         * Matter resource mappings:
         * - OnOff cluster: resourceType = "isOn", value = "true"/"false"
         * - Level cluster: resourceType = "level", value = "0-254"
         */
        bool BartonMatterImplementation::MapActionToResource(
            VoiceCommand::Action action, int levelValue,
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
                    // Toggle would require reading current state first
                    // For simplicity, default to ON
                    resourceType = "isOn";
                    value = "true";
                    LOGWARN("Toggle not fully implemented, defaulting to ON");
                    return true;
                    
                case VoiceCommand::Action::DIM:
                    resourceType = "level";
                    value = "64"; // ~25% brightness (Matter level: 0-254)
                    return true;
                    
                case VoiceCommand::Action::BRIGHTEN:
                    resourceType = "level";
                    value = "254"; // 100% brightness
                    return true;
                    
                case VoiceCommand::Action::SET_LEVEL:
                    if (levelValue >= 0 && levelValue <= 100) {
                        resourceType = "level";
                        // Convert percentage (0-100) to Matter level (0-254)
                        int matterLevel = (levelValue * 254) / 100;
                        value = std::to_string(matterLevel);
                        return true;
                    }
                    LOGERR("Invalid level value: %d", levelValue);
                    return false;
                    
                default:
                    LOGERR("Unknown action type");
                    return false;
            }
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
