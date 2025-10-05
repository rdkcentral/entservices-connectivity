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

using namespace std;

static gchar *network_ssid = NULL;
static gchar *network_psk = NULL;
std::mutex BartonMatterImplementation::networkCredsMtx;

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
		Core::hresult BartonMatterImplementation::SetWifiCredentials(const std::string ssid /* @in */, const std::string password /* @in */)
		{
			LOGWARN("BartonMatter: set wifi cred invoked");
			b_reference_network_credentials_provider_set_wifi_network_credentials(ssid.c_str(), password.c_str());
			LOGWARN("BartonMatter wifi cred processed successfully ssid: %s | pass: %s", ssid.c_str(), password.c_str());
			return (Core::ERROR_NONE);
		}
		Core::hresult BartonMatterImplementation::CommissionDevice(const std::string passcode)
		{
			LOGWARN("Commission called with passcode: %s",passcode.c_str());
			g_autofree gchar* setupPayload = g_strdup(passcode.c_str());
			bool result = Commission(bartonClient, setupPayload, 120);
			return (Core::ERROR_NONE);
		}

		bool BartonMatterImplementation::Commission(BCoreClient *client, gchar *setupPayload,guint16 timeoutSeconds)
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
			gchar* confDir = GetConfigDirectory();
			InitializeClient(confDir);
			if(!bartonClient)
			{
				LOGERR("Barton client not initliazed");
			}
			else
			{
				b_core_client_start(bartonClient);
				b_core_client_set_system_property(bartonClient, "deviceDescriptorBypass", "true");
			}
			return (Core::ERROR_NONE);
		}

		gchar* BartonMatterImplementation::GetConfigDirectory()
		{
			std::string pathStr = "/opt/.brtn-ds";
			gchar* confDir = g_strdup(pathStr.c_str());
			g_mkdir_with_parents(confDir, 0755);
			return confDir;
		}

	} // namespace Plugin
} // namespace WPEFramework

// C-style GLib implementation for network credentials provider
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

