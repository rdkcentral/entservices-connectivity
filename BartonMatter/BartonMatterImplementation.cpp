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

#include "BartonMatterImplementation.h"

using namespace std;

namespace WPEFramework
{
	namespace Plugin
	{

		std::string gPendingIdRequest("");
		std::string gPendingIdOptionsRequest("");
		std::string gPendingUrl("");
		SERVICE_REGISTRATION(BartonMatterImplementation, 1, 0);

		BartonMatterImplementation::BartonMatterImplementation()
		{
			TRACE(Trace::Information, (_T("Constructing BartonMatterImplementation Service: %p"), this));
		}

		BartonMatterImplementation::~BartonMatterImplementation()
		{
			TRACE(Trace::Information, (_T("Destructing BartonMatterImplementation Service: %p"), this));
		}

		Core::hresult BartonMatterImplementation::Initialize()
		{   
			LOGINFO("initialize called on BartonMatter implementation ");
			g_autofree gchar *confDir = GetDefaultConfigDir();
			g_autofree gchar *histFile = g_strdup_printf("%s/%s", confDir, HISTORY_FILE);

			g_autoptr(BCoreClient) client = InitializeClient(confDir);

			/*Start the client*/
			b_core_client_start(client);
			b_core_client_set_system_property(client, "deviceDescriptorBypass", "true");
			RegisterEventHandlers(client);
			return (Core::ERROR_NONE);
		}

		Core::hresult BartonMatterImplementation::Deinitialize()
		{
			LOGINFO("deinitializing BartonMatter process");
			return (Core::ERROR_NONE);
		}

		Core::hresult SetWifiCredentials(const std::string ssid /* @in */, const std::string password /* @in */)
		{
			LOGINFO("Setting wifi credentials");
			return (Core::ERROR_NONE);
		}

		Core::hresult CommissionDevice(const std::string passcode /* @in */)
		{
			LOGINFO("Starting commissioning process");
			return (Core::ERROR_NONE);
		}

		Core::hresult ReadResource()
		{
			LOGINFO("Reading the current status of the resource");
			return (Core::ERROR_NONE);
		}

		Core::hresult WriteResource()
		{
			LOGINFO("Writing the resource");
			return (Core::ERROR_NONE);
		}

		Core::hresult DisconnectDevice()
		{
			LOGINFO("Disconnecting the device");
			return (Core::ERROR_NONE);
		}

		gchar *GetDefaultConfigDir()
		{
			g_autofree gchar *confDir = g_strdup_printf("/opt/.brtn-ds");
			g_mkdir_with_parents(confDir, DEFAULT_CONF_DIR_MODE);
			return confDir;
		}

		BCoreClient *InitializeClient(gchar *confDir)
		{
			g_autoptr(BCoreInitializeParamsContainer) params = b_core_initialize_params_container_new();
			b_core_initialize_params_container_set_storage_dir(params, confDir);

			g_autofree gchar *matterConfigDir = g_strdup_printf("%s/matter", confDir);
			g_mkdir_with_parents(matterConfDir, DEFAULT_CONF_DIR_MODE);
			b_core_initialize_params_container_set_matter_storage_dir(params, matterConfDir);
			b_core_initialize_params_container_set_matter_attestation_trust_store_dir(params, matterConfDir);

			b_core_initialize_params_container_set_account_id(params, "1");
			/*implement the network credentials provider*/

			//g_autoptr(BReferenceNetworkCredentialsProvider) networkCredentialsProvider = b_reference_network_credentials_provider_new();
			//b_core_initialize_params_container_set_network_credentials_provider(params, B_CORE_NETWORK_CREDENTIALS_PROVIDER(networkCredentialsProvider));

			BCoreClient *client = b_core_client_new(params);
			BCorePropertyProvider *propProvider = b_core_initialize_params_container_get_property_provider(params);
			if(propProvider != NULL)
			{
				const gchar *disableSubsystems = "zigbee,thread";
				b_core_property_provider_set_property_string(propProvider, "device.subsystem.disable", disableSubsystems);
			}
			SetDefaultParameters(params);
			return client;
		}

		void SetDefaultParameters(BCoreInitializeParamsContainer *params)
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
		void RegisterEventHandlers(BCoreClient *client)
		{
			//register all the required callbacks
		}



	} // namespace Plugin
} // namespace WPEFramework
