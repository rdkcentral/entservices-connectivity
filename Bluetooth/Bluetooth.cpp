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

#include <fstream>

#include "Bluetooth.h"

#include "UtilsUnused.h"
#include "UtilsCStr.h"
#include "UtilsString.h"
#include "UtilsJsonRpc.h"

#include <stdlib.h>

// IMPLEMENTATION NOTE
//
// Bluetooth Settings API in Thunder follows the schema proposed by Metrological what differs from the underlying
// Bluetooth Manager API in RDK, which the plugin calls.
// As a result, the exposed (registered) methods are implemented as wrappers that follow the Metrological notation. The wrappers then call the private methods
// with RDK naming schema  which, in turn, call to actual Bluetooth Manager functions. These "internal" methods are similar to what we had in Service Manager as public APIs.
// For example, the exposed "startScan" method is mapped to "startScanWrapper()" and that one calls to "startDeviceDiscovery()" internally,
// which finally calls to "BTRMGR_StartDeviceDiscovery()" in Bluetooth Manager.

//#define BLUETOOTH_DEBUG

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 1
#define API_VERSION_NUMBER_PATCH 0

const string WPEFramework::Plugin::Bluetooth::SERVICE_NAME = "org.rdk.Bluetooth";
const string WPEFramework::Plugin::Bluetooth::METHOD_START_SCAN = "startScan";
const string WPEFramework::Plugin::Bluetooth::METHOD_STOP_SCAN = "stopScan";
const string WPEFramework::Plugin::Bluetooth::METHOD_IS_DISCOVERABLE = "isDiscoverable";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_DISCOVERED_DEVICES = "getDiscoveredDevices";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_PAIRED_DEVICES = "getPairedDevices";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_CONNECTED_DEVICES = "getConnectedDevices";
const string WPEFramework::Plugin::Bluetooth::METHOD_CONNECT = "connect";
const string WPEFramework::Plugin::Bluetooth::METHOD_DISCONNECT = "disconnect";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_AUDIO_STREAM = "setAudioStream";
const string WPEFramework::Plugin::Bluetooth::METHOD_PAIR = "pair";
const string WPEFramework::Plugin::Bluetooth::METHOD_UNPAIR = "unpair";
const string WPEFramework::Plugin::Bluetooth::METHOD_ENABLE = "enable";
const string WPEFramework::Plugin::Bluetooth::METHOD_DISABLE = "disable";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_DISCOVERABLE = "setDiscoverable";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_NAME = "getName";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_NAME = "setName";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_AUDIO_PLAYBACK_COMMAND = "sendAudioPlaybackCommand";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_EVENT_RESPONSE = "respondToEvent";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_DEVICE_INFO = "getDeviceInfo";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_AUDIO_INFO = "getAudioInfo";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_DEVICE_VOLUME_MUTE_INFO = "getDeviceVolumeMuteInfo";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_DEVICE_VOLUME_MUTE_INFO = "setDeviceVolumeMuteInfo";
const string WPEFramework::Plugin::Bluetooth::METHOD_SET_AUTO_CONNECT = "setAutoConnect";
const string WPEFramework::Plugin::Bluetooth::METHOD_GET_AUTO_CONNECT_STATUS = "getAutoConnect";
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
const string WPEFramework::Plugin::Bluetooth::METHOD_PERFORM_MIGRATION = "performMigration";
const string WPEFramework::Plugin::Bluetooth::METHOD_CLEAR_MIGRATION = "clearMigration";
#endif

const string WPEFramework::Plugin::Bluetooth::EVT_STATUS_CHANGED = "onStatusChanged";
const string WPEFramework::Plugin::Bluetooth::EVT_PAIRING_REQUEST = "onPairingRequest";
const string WPEFramework::Plugin::Bluetooth::EVT_REQUEST_FAILED = "onRequestFailed";
const string WPEFramework::Plugin::Bluetooth::EVT_CONNECTION_REQUEST = "onConnectionRequest";
const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_REQUEST = "onPlaybackRequest";

const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_STARTED = "onPlaybackChange"; // action: started
const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_PAUSED = "onPlaybackChange";  // action: paused
const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_STOPPED = "onPlaybackChange"; // action: stopped
const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_ENDED = "onPlaybackChange";   // action: paused

const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_POSITION = "onPlaybackProgress";
const string WPEFramework::Plugin::Bluetooth::EVT_PLAYBACK_NEW_TRACK = "onPlaybackNewTrack";
const string WPEFramework::Plugin::Bluetooth::EVT_DEVICE_FOUND = "onDeviceFound";
const string WPEFramework::Plugin::Bluetooth::EVT_DEVICE_LOST_OR_OUT_OF_RANGE = "onDeviceLost";
const string WPEFramework::Plugin::Bluetooth::EVT_DEVICE_DISCOVERY_UPDATE = "onDiscoveredDevice";
const string WPEFramework::Plugin::Bluetooth::EVT_DEVICE_MEDIA_STATUS = "onDeviceMediaStatus";

const string WPEFramework::Plugin::Bluetooth::STATUS_NO_BLUETOOTH_HARDWARE = "NO_BLUETOOTH_HARDWARE";
const string WPEFramework::Plugin::Bluetooth::STATUS_SOFTWARE_DISABLED = "SOFTWARE_DISABLED";
const string WPEFramework::Plugin::Bluetooth::STATUS_AVAILABLE = "AVAILABLE";
const string WPEFramework::Plugin::Bluetooth::ENABLE_CONNECT = "CONNECT";
const string WPEFramework::Plugin::Bluetooth::ENABLE_DISCONNECT = "DISCONNECT";
const string WPEFramework::Plugin::Bluetooth::ENABLE_BLUETOOTH_ENABLED = "BLUETOOTH_ENABLED";
const string WPEFramework::Plugin::Bluetooth::ENABLE_BLUETOOTH_DISABLED = "BLUETOOTH_DISABLED";
const string WPEFramework::Plugin::Bluetooth::ENABLE_BLUETOOTH_INPUT_ENABLED = "BLUETOOTH_INPUT_ENABLED";

const string WPEFramework::Plugin::Bluetooth::ENABLE_PRIMARY_AUDIO = "PRIMARY";
const string WPEFramework::Plugin::Bluetooth::ENABLE_AUXILIARY_AUDIO = "AUXILIARY";
const string WPEFramework::Plugin::Bluetooth::STATUS_HARDWARE_AVAILABLE = "HARDWARE_AVAILABLE";
const string WPEFramework::Plugin::Bluetooth::STATUS_HARDWARE_DISABLED = "HARDWARE_DISABLED";
const string WPEFramework::Plugin::Bluetooth::STATUS_SOFTWARE_ENABLED = "SOFTWARE_ENABLED";
const string WPEFramework::Plugin::Bluetooth::STATUS_SOFTWARE_INPUT_ENABLED = "SOFTWARE_INPUT_ENABLED";
const string WPEFramework::Plugin::Bluetooth::STATUS_PAIRING_CHANGE = "PAIRING_CHANGE";
const string WPEFramework::Plugin::Bluetooth::STATUS_CONNECTION_CHANGE = "CONNECTION_CHANGE";
const string WPEFramework::Plugin::Bluetooth::STATUS_DISCOVERY_STARTED = "DISCOVERY_STARTED";
const string WPEFramework::Plugin::Bluetooth::STATUS_DISCOVERY_COMPLETED = "DISCOVERY_COMPLETED";
const string WPEFramework::Plugin::Bluetooth::STATUS_PAIRING_FAILED = "PAIRING_FAILED";
const string WPEFramework::Plugin::Bluetooth::STATUS_CONNECTION_FAILED= "CONNECTION_FAILED";
const string WPEFramework::Plugin::Bluetooth::STATUS_AUTOCONNECT_STATUS_CHANGE= "AUTOCONNECT_STATUS_CHANGE";

const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_PLAY = "PLAY";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_STOP = "STOP";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_PAUSE = "PAUSE";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_RESUME = "RESUME";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_SKIP_NEXT = "SKIP_NEXT";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_SKIP_PREV = "SKIP_PREV";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_RESTART = "RESTART";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_VOLUME_UP = "VOLUME_UP";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_VOLUME_DOWN = "VOLUME_DOWN";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_MUTE = "AUDIO_MUTE";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_UNMUTE = "AUDIO_UNMUTE";
const string WPEFramework::Plugin::Bluetooth::CMD_AUDIO_CTRL_UNKNOWN = "CMD_UNKNOWN";

namespace WPEFramework
{
    namespace {

        static Plugin::Metadata<Plugin::Bluetooth> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }
    
    namespace Plugin
    {
        SERVICE_REGISTRATION(Bluetooth, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        Bluetooth* Bluetooth::_instance = nullptr;
        static Core::TimerType<DiscoveryTimer> _discoveryTimer(64 * 1024, "DiscoveryTimer");

        Bluetooth::Bluetooth()
        : PluginHost::JSONRPC()
        , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
        , m_discoveryRunning(false)
        , m_discoveryTimer(this)
        , m_powerManagerNotification(*this)
        {
            Bluetooth::_instance = this;
        }

        Bluetooth::~Bluetooth()
        {
        }

        void Bluetooth::disconnectExternallyConnectedDevices()
        {
            JsonArray connectedDevicesJson = getConnectedDevices();

            for (size_t i = 0; i < connectedDevicesJson.Length(); i++)
            {
                JsonObject device = connectedDevicesJson[i].Object();
                string deviceID = device["deviceID"].String();
                string deviceType = device["deviceType"].String();

                if (device.HasLabel("autoconnect") &&
                    !device["autoconnect"].Boolean() &&
                    deviceType != "HUMAN INTERFACE DEVICE")
                {
                    LOGINFO("Disconnecting externally connected device with deviceID=%s\n", deviceID.c_str());
                    try {
                        (void)setDeviceConnection(std::stoll(deviceID), false, deviceType);
                    } catch (const std::exception& e) {
                        LOGERR("Failed to disconnect device with deviceID=%s: %s\n", deviceID.c_str(), e.what());
                    }
                }
            }
        }

        const string Bluetooth::Initialize(PluginHost::IShell* service)
        {
            string message = "";

            Register(METHOD_GET_API_VERSION_NUMBER, &Bluetooth::getApiVersionNumber, this);
            Register(METHOD_START_SCAN, &Bluetooth::startScanWrapper, this);
            Register(METHOD_STOP_SCAN, &Bluetooth::stopScanWrapper, this);
            Register(METHOD_IS_DISCOVERABLE, &Bluetooth::isDiscoverableWrapper, this);
            Register(METHOD_GET_DISCOVERED_DEVICES, &Bluetooth::getDiscoveredDevicesWrapper, this);
            Register(METHOD_GET_PAIRED_DEVICES, &Bluetooth::getPairedDevicesWrapper, this);
            Register(METHOD_GET_CONNECTED_DEVICES, &Bluetooth::getConnectedDevicesWrapper, this);
            Register(METHOD_CONNECT, &Bluetooth::connectWrapper, this);
            Register(METHOD_DISCONNECT, &Bluetooth::disconnectWrapper, this);
            Register(METHOD_SET_AUDIO_STREAM, &Bluetooth::setAudioStreamWrapper, this);
            Register(METHOD_PAIR, &Bluetooth::pairWrapper, this);
            Register(METHOD_UNPAIR, &Bluetooth::unpairWrapper, this);
            Register(METHOD_ENABLE, &Bluetooth::enableWrapper, this);
            Register(METHOD_DISABLE, &Bluetooth::disableWrapper, this);
            Register(METHOD_SET_DISCOVERABLE, &Bluetooth::setDiscoverableWrapper, this);
            Register(METHOD_GET_NAME, &Bluetooth::getNameWrapper, this);
            Register(METHOD_SET_NAME, &Bluetooth::setNameWrapper, this);
            Register(METHOD_SET_AUDIO_PLAYBACK_COMMAND, &Bluetooth::sendAudioPlaybackCommandWrapper, this);
            Register(METHOD_SET_EVENT_RESPONSE, &Bluetooth::setEventResponseWrapper, this);
            Register(METHOD_GET_DEVICE_INFO, &Bluetooth::getDeviceInfoWrapper, this);
            Register(METHOD_GET_AUDIO_INFO, &Bluetooth::getMediaTrackInfoWrapper, this);
            Register(METHOD_GET_DEVICE_VOLUME_MUTE_INFO, &Bluetooth::getDeviceVolumeMuteInfoWrapper, this);
            Register(METHOD_SET_DEVICE_VOLUME_MUTE_INFO, &Bluetooth::setDeviceVolumeMuteInfoWrapper, this);
            Register(METHOD_SET_AUTO_CONNECT, &Bluetooth::setAutoConnectWrapper, this);
            Register(METHOD_GET_AUTO_CONNECT_STATUS, &Bluetooth::getAutoConnectWrapper, this);
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            Register(METHOD_PERFORM_MIGRATION, &Bluetooth::performMigrationWrapper, this);
            Register(METHOD_CLEAR_MIGRATION, &Bluetooth::clearMigrationWrapper, this);
#endif

            // Build EventBridge callbacks — translate SDK events to plugin notifications.
            EventBridge::Callbacks evtCbs;
            evtCbs.onStatusChanged = [this](const std::string& eventId, const std::string& newStatus,
                                            const std::string& deviceId, const std::string& name,
                                            const std::string& deviceType, uint32_t rawType,
                                            uint16_t bleType, bool paired, bool connected,
                                            bool lastConnected, bool hasAC, bool autoConnect) {
                JsonObject params;
                params["newStatus"] = newStatus;
                if (!deviceId.empty()) {
                    params["deviceID"]           = deviceId;
                    params["name"]               = name;
                    params["deviceType"]         = deviceType;
                    params["rawDeviceType"]      = std::to_string(rawType);
                    params["rawBleDeviceType"]   = std::to_string(bleType);
                    params["lastConnectedState"] = lastConnected;
                    params["paired"]             = paired;
                    params["connected"]          = connected;
                    if (hasAC) params["autoconnect"] = autoConnect;
                }
                sendNotify(C_STR(EVT_STATUS_CHANGED), params);
            };
            evtCbs.onDiscoveredDevice = [this](const std::string& deviceId, const std::string& name,
                                               const std::string& deviceType, uint32_t rawType,
                                               uint16_t bleType, bool paired, bool lastConnected,
                                               const std::string& discoveryType) {
                JsonObject params;
                params["deviceID"]           = deviceId;
                params["discoveryType"]      = discoveryType;
                params["name"]               = name;
                params["deviceType"]         = deviceType;
                params["rawDeviceType"]      = std::to_string(rawType);
                params["rawBleDeviceType"]   = std::to_string(bleType);
                params["lastConnectedState"] = lastConnected;
                params["paired"]             = paired;
                sendNotify(C_STR(EVT_DEVICE_DISCOVERY_UPDATE), params);
            };
            evtCbs.onDeviceFound = [this](const std::string& deviceId, const std::string& name,
                                          const std::string& deviceType, uint32_t rawType,
                                          uint16_t bleType, bool lastConnected) {
                JsonObject params;
                params["deviceID"]           = deviceId;
                params["name"]               = name;
                params["deviceType"]         = deviceType;
                params["rawDeviceType"]      = std::to_string(rawType);
                params["rawBleDeviceType"]   = std::to_string(bleType);
                params["lastConnectedState"] = lastConnected;
                sendNotify(C_STR(EVT_DEVICE_FOUND), params);
            };
            evtCbs.onDeviceLost = [this](const std::string& deviceId, const std::string& name,
                                         const std::string& deviceType, uint32_t rawType,
                                         uint16_t bleType, bool lastConnected) {
                JsonObject params;
                params["deviceID"]           = deviceId;
                params["name"]               = name;
                params["deviceType"]         = deviceType;
                params["rawDeviceType"]      = std::to_string(rawType);
                params["rawBleDeviceType"]   = std::to_string(bleType);
                params["lastConnectedState"] = lastConnected;
                sendNotify(C_STR(EVT_DEVICE_LOST_OR_OUT_OF_RANGE), params);
            };
            evtCbs.onRequestFailed = [this](const std::string& newStatus, const std::string& deviceId,
                                            const std::string& name, const std::string& deviceType,
                                            uint32_t rawType, uint16_t bleType, bool paired, bool connected) {
                JsonObject params;
                params["newStatus"]          = newStatus;
                params["deviceID"]           = deviceId;
                params["name"]               = name;
                params["deviceType"]         = deviceType;
                params["rawDeviceType"]      = std::to_string(rawType);
                params["rawBleDeviceType"]   = std::to_string(bleType);
                params["paired"]             = paired;
                params["connected"]          = connected;
                sendNotify(C_STR(EVT_REQUEST_FAILED), params);
            };
            evtCbs.getAutoConnect = [this](const std::string& handleStr, bool& autoConnect) -> bool {
                AutoConnectStatus status;
                if (Core::ERROR_NONE == m_bluetoothDeviceManager.getAutoConnect(handleStr, status)
                    && AUTO_CONNECT_STATUS_UNSET != status) {
                    autoConnect = (AUTO_CONNECT_STATUS_ENABLED == status);
                    return true;
                }
                return false;
            };

            // Build AuthBridge callbacks — emit auth request notifications to clients.
            AuthBridge::Callbacks authCbs;
            authCbs.onPairingRequest = [this](const std::string& deviceId, const std::string& name,
                                              const std::string& deviceType, uint32_t vendorId,
                                              const std::string& mac, const std::string& profile,
                                              bool pinRequired, uint32_t pinValue) {
                JsonObject params;
                params["deviceID"]         = deviceId;
                params["name"]             = name;
                params["deviceType"]       = deviceType;
                params["manufacturer"]     = std::to_string(vendorId);
                params["MAC"]              = mac;
                params["supportedProfile"] = profile;
                params["pinRequired"]      = pinRequired ? "true" : "false";
                if (pinRequired) params["pinValue"] = std::to_string(pinValue);
                sendNotify(C_STR(EVT_PAIRING_REQUEST), params);
            };
            authCbs.onConnectionRequest = [this](const std::string& deviceId, const std::string& name,
                                                 const std::string& deviceType, uint32_t vendorId,
                                                 const std::string& mac, const std::string& profile) {
                JsonObject params;
                params["deviceID"]         = deviceId;
                params["name"]             = name;
                params["deviceType"]       = deviceType;
                params["manufacturer"]     = std::to_string(vendorId);
                params["MAC"]              = mac;
                params["supportedProfile"] = profile;
                sendNotify(C_STR(EVT_CONNECTION_REQUEST), params);
            };
            authCbs.isPaired = [this](const std::string& handleStr) -> bool {
                AutoConnectStatus status;
                return Core::ERROR_NONE == m_bluetoothDeviceManager.getAutoConnect(handleStr, status);
            };

            message = m_btSdkAdapter.init(service, std::move(evtCbs), std::move(authCbs));
            if (!message.empty()) {
                LOGERR("%s", message.c_str());
                return message;
            }

            m_powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                .withIShell(service)
                .withRetryIntervalMS(200)
                .withRetryCount(25)
                .createInterface();

            if (m_powerManagerPlugin) {
                m_powerManagerPlugin->Register(&m_powerManagerNotification);

                WPEFramework::Exchange::IPowerManager::PowerState currentState, prevState;
                if (Core::ERROR_NONE == m_powerManagerPlugin->GetPowerState(currentState, prevState)) {
                    onPowerModeChanged(prevState, currentState);
                } else {
                    LOGERR("Failed to get current power state");
                }
            } else {
                LOGERR("Failed to get PowerManager interface");
            }

            m_bluetoothDeviceManager.setBtSdkAdapter(&m_btSdkAdapter);
            if (Core::ERROR_NONE != m_bluetoothDeviceManager.init(service)) {
                message = "Failed to initialize BluetoothDeviceManager";
                LOGERR("%s", message.c_str());
                return message;
            }

            disconnectExternallyConnectedDevices();

            return message;
        }

        void Bluetooth::Deinitialize(PluginHost::IShell* service)
        {
            m_bluetoothDeviceManager.deinit();

            if (m_powerManagerPlugin) {
                m_powerManagerPlugin->Unregister(&m_powerManagerNotification);
                m_powerManagerPlugin.Reset();
            }

            m_btSdkAdapter.deinit();

            Bluetooth::_instance = nullptr;
        }

        string Bluetooth::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }

        /// Internal methods begin
        //

        void Bluetooth::getStatusSupport(string& status)
        {
            bool powered = false;
            if (!m_btSdkAdapter.getAdapterPowered(powered)) {
                status = STATUS_NO_BLUETOOTH_HARDWARE;
            } else {
                status = powered ? STATUS_AVAILABLE : STATUS_SOFTWARE_DISABLED;
            }
            LOGINFO("getStatusSupport: returning %s", C_STR(status));
        }

        bool Bluetooth::isAdapterDiscoverable()
        {
            bool discoverable = false;
            m_btSdkAdapter.isAdapterDiscoverable(discoverable);
            return discoverable;
        }

        string Bluetooth::startDeviceDiscovery(int timeout, const string &discProfile)
        {
            if (m_discoveryRunning) {
                LOGWARN("Discovery is in progress..!");
                return STATUS_AVAILABLE;
            }

            if (!m_btSdkAdapter.startScan(discProfile)) {
                LOGERR("Failed to start the discovery..!");
                return STATUS_NO_BLUETOOTH_HARDWARE;
            }

            LOGWARN("Started discovery..!");
            m_discoveryRunning = true;

            if (timeout <= 0) {
                stopDeviceDiscovery();
            } else {
                startDiscoveryTimer(timeout * 1000);
            }

            return STATUS_AVAILABLE;
        }

        bool Bluetooth::stopDeviceDiscovery()
        {
            if (m_discoveryRunning) {
                stopDiscoveryTimer();
                bool ok = m_btSdkAdapter.stopScan();
                if (!ok) LOGERR("Failed to stop the discovery..!");
                else     LOGWARN("Stopped discovery..!");
                m_discoveryRunning = false;
                return ok;
            }
            return false;
        }

        void Bluetooth::startDiscoveryTimer(int msec)
        {
            stopDiscoveryTimer();
            _discoveryTimer.Schedule(Core::Time::Now().Add(msec), m_discoveryTimer);
        }

        void Bluetooth::stopDiscoveryTimer()
        {
            _discoveryTimer.Revoke(m_discoveryTimer);
        }

        void Bluetooth::onDiscoveryTimer()
        {
            stopDeviceDiscovery();
        }

        JsonArray Bluetooth::getDiscoveredDevices()
        {
            JsonArray deviceArray;
            for (const auto& device : m_btSdkAdapter.getDiscoveredDevices()) {
                std::string mac;
                device->address(mac);
                std::string deviceId = BtSdkAdapter::handleForMac(mac);
                std::string name;
                device->name(name);
                std::string deviceType = m_btSdkAdapter.getDeviceByHandle(deviceId)
                    ? [&]{ bluetooth::DeviceProperties p; device->getAllProperties(p); return DeviceTypeClassifier::classify(p); }()
                    : "UNKNOWN DEVICE";
                bluetooth::DeviceProperties props;
                device->getAllProperties(props);

                JsonObject deviceDetails;
                deviceDetails["deviceID"]         = deviceId;
                deviceDetails["name"]             = name;
                deviceDetails["deviceType"]       = deviceType;
                deviceDetails["connected"]        = (device->state() == bluetooth::DeviceState::Connected);
                deviceDetails["paired"]           = (device->state() == bluetooth::DeviceState::Paired
                                                  || device->state() == bluetooth::DeviceState::Connected);
                deviceDetails["rawDeviceType"]    = std::to_string(props.classOfDevice.value_or(0));
                deviceDetails["rawBleDeviceType"] = std::to_string(props.appearance.value_or(0));
                deviceArray.Add(deviceDetails);
            }
            return deviceArray;
        }

        JsonArray Bluetooth::getPairedDevices()
        {
            JsonArray deviceArray;
            for (const auto& device : m_btSdkAdapter.getPairedDevices()) {
                std::string mac;
                device->address(mac);
                const std::string deviceId = BtSdkAdapter::handleForMac(mac);

                bluetooth::DeviceProperties props;
                device->getAllProperties(props);
                std::string deviceType = DeviceTypeClassifier::classify(props);
                std::string name;
                device->name(name);

                JsonObject deviceDetails;
                deviceDetails["deviceID"]         = deviceId;
                deviceDetails["name"]             = name;
                deviceDetails["deviceType"]       = deviceType;
                deviceDetails["connected"]        = (device->state() == bluetooth::DeviceState::Connected);
                deviceDetails["rawDeviceType"]    = std::to_string(props.classOfDevice.value_or(0));
                deviceDetails["rawBleDeviceType"] = std::to_string(props.appearance.value_or(0));

                std::string lastConnectTimeUtc;
                if (Core::ERROR_NONE == m_bluetoothDeviceManager.getLastConnectTimeUtc(deviceId, lastConnectTimeUtc)
                    && !lastConnectTimeUtc.empty()) {
                    deviceDetails["lastConnectTimeUtc"] = lastConnectTimeUtc;
                }

                AutoConnectStatus autoConnectStatus;
                if (Core::ERROR_NONE == m_bluetoothDeviceManager.getAutoConnect(deviceId, autoConnectStatus)
                    && AUTO_CONNECT_STATUS_UNSET != autoConnectStatus) {
                    deviceDetails["autoconnect"] = (AUTO_CONNECT_STATUS_ENABLED == autoConnectStatus);
                }

                deviceArray.Add(deviceDetails);
            }
            return deviceArray;
        }

        JsonArray Bluetooth::getConnectedDevices()
        {
            JsonArray deviceArray;
            for (const auto& device : m_btSdkAdapter.getConnectedDevices()) {
                std::string mac;
                device->address(mac);
                const std::string deviceId = BtSdkAdapter::handleForMac(mac);

                bluetooth::DeviceProperties props;
                device->getAllProperties(props);
                std::string deviceType = DeviceTypeClassifier::classify(props);
                std::string name;
                device->name(name);

                JsonObject deviceDetails;
                deviceDetails["deviceID"]         = deviceId;
                deviceDetails["name"]             = name;
                deviceDetails["deviceType"]       = deviceType;
                deviceDetails["activeState"]      = "1"; // POWER_ACTIVE
                deviceDetails["rawDeviceType"]    = std::to_string(props.classOfDevice.value_or(0));
                deviceDetails["rawBleDeviceType"] = std::to_string(props.appearance.value_or(0));

                std::string lastConnectTimeUtc;
                if (Core::ERROR_NONE == m_bluetoothDeviceManager.getLastConnectTimeUtc(deviceId, lastConnectTimeUtc)
                    && !lastConnectTimeUtc.empty()) {
                    deviceDetails["lastConnectTimeUtc"] = lastConnectTimeUtc;
                }

                AutoConnectStatus autoConnectStatus;
                if (Core::ERROR_NONE == m_bluetoothDeviceManager.getAutoConnect(deviceId, autoConnectStatus)
                    && AUTO_CONNECT_STATUS_UNSET != autoConnectStatus) {
                    deviceDetails["autoconnect"] = (AUTO_CONNECT_STATUS_ENABLED == autoConnectStatus);
                }

                deviceArray.Add(deviceDetails);
            }
            return deviceArray;
        }

        bool Bluetooth::setDeviceConnection(long long int deviceID, bool connect, const string &deviceType)
        {
            // Connection dispatch table eliminated: SDK Device::connect/disconnect handles profile selection.
            const string deviceIdStr = std::to_string(deviceID);
            bool ok = connect ? m_btSdkAdapter.connectDevice(deviceIdStr)
                              : m_btSdkAdapter.disconnectDevice(deviceIdStr);

            if (ok && connect) {
                m_bluetoothDeviceManager.setLastConnectTimeUtc(deviceIdStr);
            } else if (!ok) {
                LOGERR("Failed to do setDeviceConnection");
            }
            return ok;
        }

        bool Bluetooth::setAudioStream(long long int deviceID, const string &audioStreamName)
        {
#ifdef BLUETOOTH_AUDIO_SUPPORT
            // TODO(T-7): delegate to SDK AUDIO_SUPPORT routing API.
            LOGWARN("setAudioStream: BLUETOOTH_AUDIO_SUPPORT not yet implemented");
#endif
            return false;
        }

        bool Bluetooth::setDevicePairing(long long int deviceID, bool pair)
        {
            const string deviceIdStr = std::to_string(deviceID);
            bool ok = pair ? m_btSdkAdapter.pairDevice(deviceIdStr)
                           : m_btSdkAdapter.unpairDevice(deviceIdStr);

            if (!ok) {
                LOGERR("Failed to do %s", (pair ? "Pair" : "Unpair"));
                return false;
            }

            Core::hresult result = pair ? m_bluetoothDeviceManager.addDevice(deviceIdStr)
                                        : m_bluetoothDeviceManager.removeDevice(deviceIdStr);

            if (Core::ERROR_NONE == result) {
                LOGINFO("Successfully done %s", (pair ? "Pair" : "Unpair"));
            } else {
                LOGERR("Failed to update cache: result=%d", result);
            }
            return true;
        }

        bool Bluetooth::setBluetoothEnabled(const string &enabled)
        {
            if (enabled == "BLUETOOTH_DISABLED") {
                return m_btSdkAdapter.setAdapterPowered(false);
            } else if (enabled == "BLUETOOTH_ENABLED") {
                return m_btSdkAdapter.setAdapterPowered(true);
            } else if (enabled == "BLUETOOTH_INPUT_ENABLED") {
                LOGERR("Bluetooth IN is not supported by STB");
            }
            return false;
        }

        bool Bluetooth::setBluetoothDiscoverable(bool enabled, int timeout)
        {
            return m_btSdkAdapter.setAdapterDiscoverable(enabled, timeout);
        }

        // Sets adapter name. No support for "power" yet
        bool Bluetooth::setBluetoothProperties(const JsonObject& parameters)
        {
            if (parameters.HasLabel("name")) {
                string name;
                getStringParameter("name", name);
                LOGWARN("Name received as %s", C_STR(name));
                return m_btSdkAdapter.setAdapterName(name);
            }
            return false;
        }

        // Gets adapter name. No support for "power" yet
        bool Bluetooth::getBluetoothProperties(JsonObject* rp)
        {
            std::string name;
            bool ok = m_btSdkAdapter.getAdapterName(name);
            if (rp) (*rp)["name"] = name;
            return ok;
        }

        bool Bluetooth::setAudioControlCommand(long long int deviceID, const string &audioCtrlCmd)
        {
#ifdef BLUETOOTH_AUDIO_SUPPORT
            // TODO(T-7): delegate to SDK AUDIO_SUPPORT media control API.
            LOGWARN("setAudioControlCommand: BLUETOOTH_AUDIO_SUPPORT not yet implemented (cmd=%s)",
                    audioCtrlCmd.c_str());
#endif
            if (audioCtrlCmd == CMD_AUDIO_CTRL_RESTART) {
                LOGERR("RESTART command is not implemented");
            }
            return false;
        }

        bool Bluetooth::setDeviceVolumeMuteProperties(long long int deviceID, const string &deviceProfile, unsigned char ui8volume, unsigned char mute)
        {
#ifdef BLUETOOTH_AUDIO_SUPPORT
            // TODO(T-7): delegate to SDK AUDIO_SUPPORT volume API.
#endif
            return false;
        }

        JsonObject Bluetooth::getDeviceVolumeMuteProperties(long long int deviceID, const string &deviceProfile)
        {
#ifdef BLUETOOTH_AUDIO_SUPPORT
            // TODO(T-7): delegate to SDK AUDIO_SUPPORT volume API.
#endif
            return JsonObject();
        }

        bool Bluetooth::setEventResponse(long long int deviceID, const string &eventType, const string &respValue)
        {
            // Resolve the pending auth request in AuthBridge using the device MAC address.
            // DeviceRegistry reverse-lookup: handle string → MAC.
            const string deviceIdStr = std::to_string(deviceID);
            auto device = m_btSdkAdapter.getDeviceByHandle(deviceIdStr);
            std::string mac;
            if (device) {
                device->address(mac);
            }

            bool accepted = Utils::String::equal(respValue, "ACCEPTED");

            if (!mac.empty() &&
                (eventType == EVT_PAIRING_REQUEST ||
                 eventType == EVT_CONNECTION_REQUEST ||
                 eventType == EVT_PLAYBACK_REQUEST)) {
                m_btSdkAdapter.respondToEvent(mac, accepted);
                LOGINFO("Successfully done setEventResponse for deviceID=%lld, accepted=%d",
                        deviceID, static_cast<int>(accepted));
                return true;
            }

            LOGERR("setEventResponse: unknown event type or device not found for deviceID=%lld", deviceID);
            return false;
        }

        JsonObject Bluetooth::getDeviceInfo(long long int deviceID)
        {
            JsonObject deviceDetails;
            const string deviceIdStr = std::to_string(deviceID);

            bluetooth::DeviceProperties props;
            if (!m_btSdkAdapter.getDeviceProperties(deviceIdStr, props)) {
                LOGERR("Failed to get device details for deviceID=%lld", deviceID);
                return deviceDetails;
            }

            std::string mac, name;
            if (props.address.has_value()) mac  = props.address.value();
            if (props.name.has_value())    name = props.name.value();

            auto device = m_btSdkAdapter.getDeviceByHandle(deviceIdStr);

            deviceDetails["deviceID"]         = deviceIdStr;
            deviceDetails["name"]             = name;
            deviceDetails["deviceType"]       = DeviceTypeClassifier::classify(props);
            deviceDetails["manufacturer"]     = std::to_string(
                props.manufacturerData.has_value() && !props.manufacturerData.value().empty()
                    ? props.manufacturerData.value().begin()->first : 0);
            deviceDetails["MAC"]              = mac;
            deviceDetails["signalStrength"]   = "0";
            if (props.rssi.has_value()) {
                deviceDetails["signalStrength"] = std::to_string(props.rssi.value());
                deviceDetails["rssi"]           = std::to_string(props.rssi.value());
            }
            deviceDetails["batteryLevel"]     = std::to_string(
                props.batteryLevel.has_value() ? props.batteryLevel.value() : 0);
            deviceDetails["modalias"]         = props.modalias.value_or("");
            deviceDetails["firmwareRevision"] = "";
            deviceDetails["supportedProfile"] = "";

            if (props.uuids.has_value()) {
                std::string profileInfo;
                for (const auto& uuid : props.uuids.value()) {
                    if (!profileInfo.empty()) profileInfo += ";";
                    profileInfo += uuid;
                }
                deviceDetails["supportedProfile"] = profileInfo;
            }

            return deviceDetails;
        }

        JsonObject Bluetooth::getMediaTrackInfo(long long int deviceID)
        {
#ifdef BLUETOOTH_AUDIO_SUPPORT
            // TODO(T-7): delegate to SDK AUDIO_SUPPORT track info API.
#endif
            return JsonObject();
        }

        //
        /// Internal methods end

        /// Registered methods begin
        //
        uint32_t Bluetooth::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            response["version"] = m_apiVersionNumber;
            returnResponse(true);
        }

        uint32_t Bluetooth::startScanWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            int timeout = -1;
            string profile;
            bool timeoutDefined = false;
            bool profileDefined = false;
            bool successFlag;
            if (parameters.HasLabel("timeout"))
            {
                getNumberParameter("timeout", timeout);
                timeoutDefined = true;
            }

            if (parameters.HasLabel("profile"))
            {
                getStringParameter("profile", profile);
                profileDefined = true;
            }
            if (timeoutDefined && profileDefined)
            {
                LOGINFO("Making a call with timeout=%d sec profile=%s", timeout, profile.c_str());
                response["status"] = startDeviceDiscovery(timeout, profile);
                successFlag = true;
            } else if (timeoutDefined) {
                LOGINFO("Making a call with timeout=%d sec", timeout);
                response["status"] = startDeviceDiscovery(timeout);
                successFlag = true;
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"timeout\": \"5\", \"profile\": \"SMARTPHONE\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::stopScanWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            stopDeviceDiscovery();
            returnResponse(true);
        }

        uint32_t Bluetooth::isDiscoverableWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            response["discoverable"] = isAdapterDiscoverable();
            returnResponse(true);
        }

        uint32_t Bluetooth::setDiscoverableWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            bool discoverable = false;
            int timeout;

            if (parameters.HasLabel("timeout"))
            {
                getNumberParameter("timeout", timeout);
            } else {
                timeout = -1;
            }

            if (parameters.HasLabel("discoverable")) {
                getBoolParameter("discoverable", discoverable);
                LOGINFO("Making a call with discoverable: %s timeout=%d", discoverable ? "YES" : "NO", timeout);
                successFlag = setBluetoothDiscoverable(discoverable, timeout);
            } else {
                LOGERR("Please specify parameters. Example (timeout is optional): \"params\": {\"discoverable\": true, \"timeout\": \"10\"}");
                successFlag = false;
            }

            returnResponse(successFlag);
        }

        uint32_t Bluetooth::getDiscoveredDevicesWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            response["discoveredDevices"] = getDiscoveredDevices();
            returnResponse(true);
        }

        uint32_t Bluetooth::getPairedDevicesWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            response["pairedDevices"] = getPairedDevices();
            returnResponse(true);
        }

        uint32_t Bluetooth::getConnectedDevicesWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            response["connectedDevices"] = getConnectedDevices();
            returnResponse(true);
        }

        uint32_t Bluetooth::connectWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string deviceType;
            bool deviceTypeDefined = false;
            bool successFlag;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if (parameters.HasLabel("deviceType"))
            {
                getStringParameter("deviceType", deviceType);
                deviceTypeDefined = true;
            } else {
                deviceType = "SMARTPHONE";
            }

            if (deviceIDDefined && deviceTypeDefined)
            {
                LOGINFO("Making a call with deviceID=%llu enable=%s deviceType=%s", deviceID, "CONNECT", deviceType.c_str());
                successFlag = setDeviceConnection(deviceID, true, deviceType);
            } else if (deviceIDDefined) {
                LOGINFO("Making a call with deviceID=%llu enable=%s", deviceID, "CONNECT");
                successFlag = setDeviceConnection(deviceID, true);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::disconnectWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string deviceType;
            bool deviceTypeDefined = false;
            bool successFlag;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if (parameters.HasLabel("deviceType"))
            {
                getStringParameter("deviceType", deviceType);
                deviceTypeDefined = true;
            } else {
                deviceType = "SMARTPHONE";
            }

            if (deviceIDDefined && deviceTypeDefined)
            {
                LOGINFO("Making a call with deviceID=%llu enable=%s deviceType=%s", deviceID, "DISCONNECT", deviceType.c_str());
                successFlag = setDeviceConnection(deviceID, false, deviceType);
            } else if (deviceIDDefined) {
                LOGINFO("Making a call with deviceID=%llu enable=%s", deviceID, "DISCONNECT");
                successFlag = setDeviceConnection(deviceID, false);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::setAudioStreamWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string audioStreamName; //"PRIMARY" or "AUXILIARY"
            bool audioStreamNameDefined = false;
            bool successFlag;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if (parameters.HasLabel("audioStreamName"))
            {
                getStringParameter("audioStreamName", audioStreamName);
                audioStreamNameDefined = true;
            }
            if (deviceIDDefined && audioStreamNameDefined)
            {
                LOGINFO("Making a call with deviceID=%llu audioStreamName=%s", deviceID, audioStreamName.c_str());
                successFlag = setAudioStream(deviceID, audioStreamName);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\", \"audioStreamName\": \"PRIMARY\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::pairWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            bool pair = true;

            if (parameters.HasLabel("deviceID")) {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if(deviceIDDefined)
            {
                LOGINFO("Making a call with deviceID=%llu pair=%s", deviceID, pair?"true":"false");
                successFlag = setDevicePairing(deviceID, pair);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::unpairWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            bool pair = false;

            if (parameters.HasLabel("deviceID")) {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if(deviceIDDefined)
            {
                LOGINFO("Making a call with deviceID=%llu pair=%s", deviceID, pair?"true":"false");
                successFlag = setDevicePairing(deviceID, pair);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::enableWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string enabled = ENABLE_BLUETOOTH_ENABLED;
            successFlag = setBluetoothEnabled(enabled);
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::disableWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string enabled = ENABLE_BLUETOOTH_DISABLED;
            successFlag = setBluetoothEnabled(enabled);
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::getNameWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            successFlag = getBluetoothProperties(&response);
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::setNameWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            successFlag = setBluetoothProperties(parameters);
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::sendAudioPlaybackCommandWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string audioCtrlCmd; // see CMD_AUDIO_CTRL_ entries
            bool audioCtrlCmdDefined = false;
            bool successFlag;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if (parameters.HasLabel("command"))
            {
                getStringParameter("command", audioCtrlCmd);
                audioCtrlCmdDefined = true;
            }

            if (deviceIDDefined && audioCtrlCmdDefined)
            {
                LOGINFO("Making a call with deviceID=%llu audioCtrlCmd=%s", deviceID, audioCtrlCmd.c_str());
                successFlag = setAudioControlCommand(deviceID, audioCtrlCmd);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\", \"command\": \"PLAY\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }


        uint32_t Bluetooth::getDeviceVolumeMuteInfoWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string deviceTypeStr;
            bool deviceTypeDefined = false;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }
            if (parameters.HasLabel("deviceType"))
            {
                getStringParameter("deviceType", deviceTypeStr);
                deviceTypeDefined = true;
            }
            if (deviceIDDefined && deviceTypeDefined)
            {
                LOGINFO("Making a call with deviceID=%llu ", deviceID);
                response ["volumeinfo"] = getDeviceVolumeMuteProperties(deviceID, deviceTypeStr);
                successFlag = true;
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\", \"deviceType\": \"HEADPHONES\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::setDeviceVolumeMuteInfoWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool successFlag;
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string deviceTypeStr;
            bool deviceTypeDefined = false;
            unsigned char ui8volume = 0;
            int ivolume = 0;
            bool volumeDefined = false;
            unsigned char mute = '0';
            int imute = 0;
            bool muteDefined = false;


            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }
            if (parameters.HasLabel("deviceType"))
            {
                getStringParameter("deviceType", deviceTypeStr);
                deviceTypeDefined = true;
            }
            if (parameters.HasLabel("volume"))
            {
                getNumberParameterObject(parameters, "volume", ivolume);
                ui8volume = static_cast<unsigned char>(ivolume);
                volumeDefined = true;
            }
            if (parameters.HasLabel("mute"))
            {
                getNumberParameterObject(parameters, "mute", imute);
                mute = static_cast<unsigned char>(imute);
                muteDefined = true;
            }

            if (deviceIDDefined && deviceTypeDefined && volumeDefined && muteDefined)
            {
                LOGINFO("Making a call with deviceID=%llu ", deviceID);
                successFlag = setDeviceVolumeMuteProperties(deviceID, deviceTypeStr, ui8volume, mute);
                if (successFlag) {
                    m_bluetoothDeviceManager.setLastVolumeSetting(deviceIDStr, static_cast<long long>(ui8volume));
                }
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\", \"deviceType\": \"HEADPHONES\", \"volume\": \"0-255\", \"mute\": \"0-1\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::setEventResponseWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool deviceIDDefined = false;
            string eventType; // see EVT_ definitions, e.g. EVT_PAIRING_REQUEST
            bool eventTypeDefined = false;
            bool successFlag;
            string responseValue;
            bool responseValueDefined = false;

            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                deviceIDDefined = true;
            }

            if (parameters.HasLabel("eventType"))
            {
                getStringParameter("eventType", eventType);
                eventTypeDefined = true;
            }

            if (parameters.HasLabel("responseValue"))
            {
                getStringParameter("responseValue", responseValue);
                responseValueDefined = true;
            }

            if(deviceIDDefined && responseValueDefined && eventTypeDefined)
            {
                LOGINFO("Making a call with deviceID=%llu eventType=%s responseValue=%s", deviceID, C_STR(eventType), C_STR(responseValue));
                successFlag = setEventResponse(deviceID, eventType, responseValue);
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\", \"eventType\": \"pairingRequest\", \"responseValue\": \"ACCEPTED\"}");
                successFlag = false;
            }

            returnResponse(successFlag);
        }

        uint32_t Bluetooth::getDeviceInfoWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool successFlag;
            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                response["deviceInfo"] = getDeviceInfo(deviceID);
                successFlag = true;
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::getMediaTrackInfoWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceIDStr;
            long long int deviceID = 0;
            bool successFlag;
            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceIDStr);
                deviceID = stoll(deviceIDStr);
                response["trackInfo"] = getMediaTrackInfo(deviceID);
                successFlag = true;
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"271731989589742\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::setAutoConnectWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceID;
            bool enable;
            bool successFlag = true;
            if (parameters.HasLabel("deviceID") && parameters.HasLabel("enable"))
            {
                getStringParameter("deviceID", deviceID);
                getBoolParameter("enable", enable);
                Core::hresult result = m_bluetoothDeviceManager.setAutoConnect(deviceID, enable);
                if (Core::ERROR_NONE != result) {
                    LOGERR("Failed to set autoConnect status for deviceID=%s, result=0x%08X", deviceID.c_str(), result);
                    successFlag = false;
                } else {
                    notifyAutoConnectStatusChanged(deviceID, enable);
                }
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"1234567890\", \"enable\": true}");
                successFlag = false;
            }
            
            returnResponse(successFlag);
        }

        uint32_t Bluetooth::getAutoConnectWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            string deviceID;
            bool successFlag = true;
            if (parameters.HasLabel("deviceID"))
            {
                getStringParameter("deviceID", deviceID);
                AutoConnectStatus status;
                Core::hresult result = m_bluetoothDeviceManager.getAutoConnect(deviceID, status);

                if (Core::ERROR_NONE == result) {
                    response["autoconnect"] = (AUTO_CONNECT_STATUS_ENABLED == status);
                } else {
                    successFlag = false;
                    LOGWARN("Failed to get autoConnect status for deviceID=%s, result=0x%08X", deviceID.c_str(), result);
                }
            } else {
                LOGERR("Please specify parameters. Example: \"params\": {\"deviceID\": \"1234567890\"}");
                successFlag = false;
            }
            returnResponse(successFlag);
        }

        //
        /// Registered methods end

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
        uint32_t Bluetooth::performMigrationWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            const Core::hresult result = m_bluetoothDeviceManager.performMigration();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration failed, hresult=%d", result);
            }
            returnResponse(Core::ERROR_NONE == result);
        }

        uint32_t Bluetooth::clearMigrationWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            UNUSED(parameters);
            const Core::hresult result = m_bluetoothDeviceManager.clearMigration();
            if (Core::ERROR_NONE != result) {
                LOGERR("clearMigration failed, hresult=%d", result);
            }
            returnResponse(Core::ERROR_NONE == result);
        }
#endif

        void Bluetooth::onPowerModeChanged(const WPEFramework::Exchange::IPowerManager::PowerState currentState, const WPEFramework::Exchange::IPowerManager::PowerState newState)
        {
            #ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                if (!m_bluetoothDeviceManager.isMigrated()) {
                    return;
                }
            #else
                return;
            #endif

            #ifdef BLUETOOTH_DEBUG
                static const char* powerStateNames[] = {
                    "POWER_STATE_UNKNOWN",
                    "POWER_STATE_OFF",
                    "POWER_STATE_STANDBY",
                    "POWER_STATE_ON",
                    "POWER_STATE_STANDBY_LIGHT_SLEEP",
                    "POWER_STATE_STANDBY_DEEP_SLEEP"
                };

                LOGINFO("%s --> %s\n", powerStateNames[currentState], powerStateNames[newState]);
            #else
                LOGINFO("Power mode changed: %d --> %d\n", currentState, newState);
            #endif

            if (newState == currentState) {
                LOGINFO("Power state unchanged, ignoring transition\n");
                return;
            }

            // ON --> OFF
            if ((WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_ON == currentState ||
                WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_UNKNOWN == currentState) &&
                (WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_OFF == newState ||
                    WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_STANDBY == newState ||
                    WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_STANDBY_LIGHT_SLEEP == newState)) {

                std::unordered_map<std::string, BluetoothDeviceInfo> pairedDeviceInfos = m_bluetoothDeviceManager.getPairedDeviceInfos();

                for (const auto& entry : pairedDeviceInfos) {
                    const std::string& deviceIdStr = entry.first;
                    const BluetoothDeviceInfo& deviceInfo = entry.second;
                    LOGINFO("pairedDeviceInfos[%s] = { deviceType=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s }\n",
                            deviceIdStr.c_str(), deviceInfo.deviceType.c_str(), static_cast<int>(deviceInfo.autoConnectStatus), deviceInfo.lastConnectTimeUtc.c_str());

                    if (deviceInfo.deviceType == "HUMAN INTERFACE DEVICE") {
                        // Don't disconnect RCU devices on power off/standby, as they are needed to wake up the device.
                        continue;
                    }

                    if (deviceInfo.autoConnectStatus == AutoConnectStatus::AUTO_CONNECT_STATUS_DISABLED) {
                        // Only disconnect if autoConnect was explicitly set false to preserve backward compatibility.
                        try {
                            long long int deviceId = stoll(deviceIdStr);
                            bool bSuccess = setDeviceConnection(deviceId, false, deviceInfo.deviceType);
                            LOGINFO("POWER OFF/STANDBY: Disconnecting deviceID=%lld, success=%s\n", deviceId, bSuccess ? "true" : "false");
                        } catch (const std::exception& e) {
                            LOGERR("Failed to parse deviceId: %s\n", e.what());
                        }
                    }
                }
            }
            // X --> ON
            else if (WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_ON == newState ) {
                std::unordered_map<std::string, BluetoothDeviceInfo> pairedDeviceInfos = m_bluetoothDeviceManager.getPairedDeviceInfos();

                LOGINFO("pairedDeviceInfos.size()=%zu\n", pairedDeviceInfos.size());

                uint16_t pairedDevicesCount = 0;

                for (const auto& entry : pairedDeviceInfos) {
                    const std::string& deviceIdStr = entry.first;
                    const BluetoothDeviceInfo& deviceInfo = entry.second;
                    LOGINFO("pairedDeviceInfos[%s] = { deviceType=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s }\n",
                            deviceIdStr.c_str(), deviceInfo.deviceType.c_str(), static_cast<int>(deviceInfo.autoConnectStatus), deviceInfo.lastConnectTimeUtc.c_str());
                    
                    if (deviceInfo.deviceType != "HUMAN INTERFACE DEVICE") {
                        ++pairedDevicesCount;
                    }
                }

                if (pairedDevicesCount > 0) {
                    setBluetoothEnabled(ENABLE_BLUETOOTH_ENABLED);
                }
            }
            // X --> DEEP_SLEEP
            else if (WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_STANDBY_DEEP_SLEEP == newState ) {

                std::unordered_map<std::string, BluetoothDeviceInfo> pairedDeviceInfos = m_bluetoothDeviceManager.getPairedDeviceInfos();

                LOGINFO("pairedDeviceInfos.size()=%zu\n", pairedDeviceInfos.size());

                for (const auto& entry : pairedDeviceInfos) {
                    const std::string& deviceIdStr = entry.first;
                    const BluetoothDeviceInfo& deviceInfo = entry.second;
                    LOGINFO("pairedDeviceInfos[%s] = { deviceType=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s }\n",
                            deviceIdStr.c_str(), deviceInfo.deviceType.c_str(), static_cast<int>(deviceInfo.autoConnectStatus), deviceInfo.lastConnectTimeUtc.c_str());

                    if (deviceInfo.deviceType == "HUMAN INTERFACE DEVICE") {
                        // Don't disconnect RCU devices when entering DEEP_SLEEP, as they are needed to wake up the device.
                        continue;
                    }

                    try {
                        long long int deviceId = std::stoll(deviceIdStr);
                        bool bSuccess = setDeviceConnection(deviceId, false, deviceInfo.deviceType);
                        LOGINFO("POWER_STATE_STANDBY_DEEP_SLEEP: Disconnecting deviceId=%lld, success=%s\n", deviceId, bSuccess ? "true" : "false");
                    } catch (const std::exception& e) {
                        LOGERR("Failed to parse deviceId: %s\n", e.what());
                    }
                }
            } else {
                LOGWARN("Unhandled transition\n");
            }
        }

        void Bluetooth::notifyAutoConnectStatusChanged(const string& deviceID, const bool enable)
        {
            JsonObject params;
            params["deviceID"] = deviceID;
            params["autoconnect"] = enable;
            params["newStatus"] = STATUS_AUTOCONNECT_STATUS_CHANGE;
            sendNotify(C_STR(EVT_STATUS_CHANGED), params);
        }

        uint64_t DiscoveryTimer::Timed(const uint64_t scheduledTime)
        {
            uint64_t result = 0;
            m_bt->onDiscoveryTimer();
            return(result);
        }
    } // Plugin
} // WPEFramework
