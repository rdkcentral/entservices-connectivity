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

#include "NetworkCommissioningDriver.h"
#include "UtilsLogging.h"

#include <app/clusters/network-commissioning/network-commissioning.h>
#include <platform/NetworkCommissioningDriver.h>

namespace WPEFramework {
namespace Plugin {

using namespace chip;
using namespace chip::app::Clusters::NetworkCommissioning;

// WiFi driver implementation for commissioner/commissionee device
// Device acts as commissioner when pairing other devices, and as commissionee when paired by Alexa
class BartonWiFiDriver : public chip::DeviceLayer::NetworkCommissioning::WiFiDriver
{
public:
    // Required interface implementations
    CHIP_ERROR Init(chip::DeviceLayer::NetworkCommissioning::Internal::BaseDriver::NetworkStatusChangeCallback * callback) override
    {
        mStatusChangeCallback = callback;
        ChipLogProgress(AppServer, "BartonWiFiDriver initialized");
        return CHIP_NO_ERROR;
    }

    void Shutdown() override
    {
        mStatusChangeCallback = nullptr;
    }

    uint8_t GetMaxNetworks() override { return 1; }
    uint8_t GetScanNetworkTimeoutSeconds() override { return 10; }
    uint8_t GetConnectNetworkTimeoutSeconds() override { return 20; }

    CHIP_ERROR CommitConfiguration() override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: CommitConfiguration called");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR RevertConfiguration() override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: RevertConfiguration called");
        return CHIP_NO_ERROR;
    }

    Status RemoveNetwork(chip::ByteSpan networkId, chip::MutableCharSpan & outDebugText, uint8_t & outNetworkIndex) override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: RemoveNetwork called");
        return Status::kSuccess;
    }

    Status ReorderNetwork(chip::ByteSpan networkId, uint8_t index, chip::MutableCharSpan & outDebugText) override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: ReorderNetwork called");
        return Status::kSuccess;
    }

    void ConnectNetwork(chip::ByteSpan networkId, ConnectCallback * callback) override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: ConnectNetwork called");
        // Device is already connected to WiFi via system configuration
        // Report success since we're already online
        if (callback)
        {
            callback->OnResult(Status::kSuccess, chip::CharSpan(), 0);
        }
    }

    void ScanNetworks(chip::ByteSpan ssid, ScanCallback * callback) override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: ScanNetworks called");
        // Return empty scan results - device manages WiFi at OS level
        if (callback)
        {
            callback->OnFinished(Status::kSuccess, chip::CharSpan(), nullptr);
        }
    }

    CHIP_ERROR AddOrUpdateNetwork(chip::ByteSpan ssid, chip::ByteSpan credentials, chip::MutableCharSpan & outDebugText,
                                   uint8_t & outNetworkIndex) override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: AddOrUpdateNetwork called (SSID len=%u)", static_cast<unsigned>(ssid.size()));
        // Device WiFi is managed at OS level, but accept the configuration for Matter commissioning
        outNetworkIndex = 0;
        return CHIP_NO_ERROR;
    }

    void OnNetworkStatusChange() override
    {
        ChipLogProgress(AppServer, "BartonWiFiDriver: OnNetworkStatusChange called");
    }

    chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap> GetSecurityTypes() override
    {
        return chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap>(
            chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap::kWpa2Personal |
            chip::app::Clusters::NetworkCommissioning::WiFiSecurityBitmap::kWpa3Personal);
    }

    chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap> GetWiFiBands() override
    {
        return chip::BitFlags<chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap>(
            chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap::k2g4 |
            chip::app::Clusters::NetworkCommissioning::WiFiBandBitmap::k5g);
    }

private:
    chip::DeviceLayer::NetworkCommissioning::Internal::BaseDriver::NetworkStatusChangeCallback * mStatusChangeCallback = nullptr;
};

// Static instance for WiFi driver
static BartonWiFiDriver sWiFiDriver;

// Static instance for the NetworkCommissioning cluster
static Instance sNetworkCommissioningInstance(0 /* endpoint */, &sWiFiDriver);

void InitializeNetworkCommissioning()
{
    LOGINFO("Initializing NetworkCommissioning cluster on endpoint 0");

    CHIP_ERROR err = sNetworkCommissioningInstance.Init();
    if (err != CHIP_NO_ERROR)
    {
        LOGERR("Failed to initialize NetworkCommissioning instance: %s", chip::ErrorStr(err));
        return;
    }

    LOGINFO("NetworkCommissioning cluster initialized successfully with WiFi driver");
}

} // namespace Plugin
} // namespace WPEFramework
