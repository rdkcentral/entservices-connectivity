/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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

#include <memory>
#include <string>
#include <vector>

#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>
#include <bluetooth/Manager.h>
#include <Status.h>

#include "AuthBridge.h"
#include "DeviceRegistry.h"
#include "DeviceTypeClassifier.h"
#include "EventBridge.h"

namespace WPEFramework {
namespace PluginHost {
class IShell;
}
namespace Plugin {

/**
 * Owns the bluetooth::Manager/Adapter lifecycle and exposes the operational
 * interface previously obtained via BTRMGR_* calls.
 *
 * Replaces: IARM init, BTRMGR_RegisterForCallbacks, BTRMGR_RegisterEventCallback,
 *           and all direct BTRMGR_* C-API calls throughout Bluetooth.cpp.
 */
class BtSdkAdapter {
public:
    BtSdkAdapter() = default;
    ~BtSdkAdapter() = default;
    BtSdkAdapter(const BtSdkAdapter&) = delete;
    BtSdkAdapter& operator=(const BtSdkAdapter&) = delete;

    // Initialise Manager, acquire default Adapter, register events.
    // Returns non-empty error string on fatal failure (no adapter found).
    std::string init(PluginHost::IShell* service,
                     EventBridge::Callbacks eventCallbacks,
                     AuthBridge::Callbacks authCallbacks);

    void deinit();

    // ── Adapter operations ──────────────────────────────────────────────────

    bool getAdapterPowered(bool& powered) const;
    bool setAdapterPowered(bool powered);
    bool getAdapterName(std::string& name) const;
    bool setAdapterName(const std::string& name);
    bool isAdapterDiscoverable(bool& discoverable) const;
    bool setAdapterDiscoverable(bool discoverable, int timeoutSeconds);

    // ── Discovery ───────────────────────────────────────────────────────────

    // Starts discovery using a profile string (same format as current plugin).
    // Returns false on failure or if already scanning.
    bool startScan(const std::string& profile);
    bool stopScan();

    // ── Device lists ────────────────────────────────────────────────────────

    std::vector<std::shared_ptr<bluetooth::Device>> getDiscoveredDevices() const;
    std::vector<std::shared_ptr<bluetooth::Device>> getPairedDevices() const;
    std::vector<std::shared_ptr<bluetooth::Device>> getConnectedDevices() const;

    // ── Device operations ───────────────────────────────────────────────────

    // Pair/unpair: synchronous, emits failure via EventBridge on error.
    bool pairDevice(const std::string& handleStr);
    bool unpairDevice(const std::string& handleStr);

    // Connect/disconnect: synchronous via device->connect(sync=true).
    // On failure, EventBridge emits onRequestFailed(CONNECTION_FAILED).
    bool connectDevice(const std::string& handleStr);
    bool disconnectDevice(const std::string& handleStr);

    // Device properties.
    bool getDeviceProperties(const std::string& handleStr,
                             bluetooth::DeviceProperties& props) const;

    // ── Registry access ─────────────────────────────────────────────────────

    // Look up a Device by its handle string for use by BluetoothDeviceManager.
    std::shared_ptr<bluetooth::Device> getDeviceByHandle(const std::string& handleStr) const;

    // Derive the handle string for a MAC address.
    static std::string handleForMac(const std::string& mac) {
        return DeviceRegistry::deriveHandle(mac);
    }

    // ── Auth ────────────────────────────────────────────────────────────────

    // Forward respondToEvent decision to AuthBridge.
    void respondToEvent(const std::string& mac, bool accepted);

private:
    void onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data);
    void registerDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    void unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    bluetooth::ScanFilter buildScanFilter(const std::string& profile) const;

    std::unique_ptr<bluetooth::Manager> m_manager;
    std::shared_ptr<bluetooth::Adapter> m_adapter;

    DeviceRegistry m_registry;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<AuthBridge>  m_authBridge;
};

} // namespace Plugin
} // namespace WPEFramework
