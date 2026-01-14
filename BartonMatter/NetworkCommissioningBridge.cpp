/**
 * NetworkCommissioning Bridge
 *
 * This file provides a stable ABI bridge for NetworkCommissioning initialization.
 * It creates the platform WiFi driver and cluster instance using C-style function
 * pointers to avoid ABI incompatibilities across compilation boundaries.
 *
 * IMPORTANT: This file must be compiled with the SAME compiler flags as the
 * Matter SDK to ensure binary compatibility.
 */

#include <platform/CHIPDeviceLayer.h>
#include <platform/Linux/NetworkCommissioningDriver.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <lib/support/logging/CHIPLogging.h>

namespace {
    // Platform WiFi driver instance - statically allocated
    chip::DeviceLayer::NetworkCommissioning::LinuxWiFiDriver sWiFiDriver;
    
    // NetworkCommissioning cluster instance wrapper
    chip::Optional<chip::app::Clusters::NetworkCommissioning::Instance> sWiFiNetworkCommissioningInstance;
    
    bool sInitialized = false;
}

// C-style API to avoid name mangling and provide stable ABI
extern "C" {

/**
 * Initialize NetworkCommissioning WiFi driver for the given endpoint
 *
 * @param endpoint The endpoint ID to register NetworkCommissioning on (typically 0)
 * @return 0 on success, -1 on failure
 */
int barton_network_commissioning_init(uint16_t endpoint)
{
    if (sInitialized)
    {
        ChipLogProgress(AppServer, "NetworkCommissioning already initialized");
        return 0;
    }

    try
    {
        ChipLogProgress(AppServer, "Initializing NetworkCommissioning WiFi driver on endpoint %u", endpoint);

        // Create and initialize the NetworkCommissioning instance with platform WiFi driver
        sWiFiNetworkCommissioningInstance.Emplace(static_cast<chip::EndpointId>(endpoint), &sWiFiDriver);
        sWiFiNetworkCommissioningInstance.Value().Init();

        sInitialized = true;
        ChipLogProgress(AppServer, "NetworkCommissioning initialized successfully");
        return 0;
    }
    catch (...)
    {
        ChipLogError(AppServer, "Failed to initialize NetworkCommissioning");
        return -1;
    }
}

/**
 * Shutdown NetworkCommissioning
 */
void barton_network_commissioning_shutdown()
{
    if (!sInitialized)
    {
        return;
    }

    ChipLogProgress(AppServer, "Shutting down NetworkCommissioning");
    sWiFiNetworkCommissioningInstance.ClearValue();
    sInitialized = false;
}

} // extern "C"
