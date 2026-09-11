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

#include "EventBridge.h"

#include <UtilsLogging.h>

namespace WPEFramework {
namespace Plugin {

namespace {

void populateDeviceFields(std::shared_ptr<bluetooth::Device> device,
                          DeviceRegistry& registry,
                          std::string& outId, std::string& outName,
                          std::string& outType, uint32_t& outRaw, uint16_t& outBle) {
    std::string mac;
    device->address(mac);
    outId = registry.getHandleForMac(mac);
    if (outId.empty()) outId = DeviceRegistry::deriveHandle(mac);

    device->name(outName);

    outType = registry.getDeviceType(outId);
    if (outType.empty()) {
        bluetooth::DeviceProperties props;
        if (device->getAllProperties(props)) {
            outType = DeviceTypeClassifier::classify(props.appearance.value_or(0),
                                                      props.classOfDevice.value_or(0),
                                                      props.uuids.value_or(std::vector<std::string>{}));
            registry.setDeviceType(outId, outType);
            outRaw = props.classOfDevice.value_or(0);
            outBle = props.appearance.value_or(0);
            return;
        }
        outType = "UNKNOWN DEVICE";
    }
    // Retrieve raw fields from properties if needed.
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        outRaw = props.classOfDevice.value_or(0);
        outBle = props.appearance.value_or(0);
    }
}

} // namespace

void EventBridge::onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data) {
    switch (event) {
        case bluetooth::AdapterEvent::DiscoveryStarted:
            if (m_callbacks.onStatusChanged) {
                m_callbacks.onStatusChanged("onStatusChanged", "DISCOVERY_STARTED",
                    "", "", "", 0, 0, false, false, false, false, false);
            }
            break;

        case bluetooth::AdapterEvent::DiscoveryStopped:
            if (m_callbacks.onStatusChanged) {
                m_callbacks.onStatusChanged("onStatusChanged", "DISCOVERY_COMPLETED",
                    "", "", "", 0, 0, false, false, false, false, false);
            }
            break;

        case bluetooth::AdapterEvent::PoweredOn:
            if (m_callbacks.onStatusChanged) {
                m_callbacks.onStatusChanged("onStatusChanged", "HARDWARE_AVAILABLE",
                    "", "", "", 0, 0, false, false, false, false, false);
            }
            break;

        case bluetooth::AdapterEvent::PoweredOff:
            if (m_callbacks.onStatusChanged) {
                m_callbacks.onStatusChanged("onStatusChanged", "SOFTWARE_DISABLED",
                    "", "", "", 0, 0, false, false, false, false, false);
            }
            break;

        case bluetooth::AdapterEvent::DeviceDiscovered: {
            if (!data.device) break;
            std::string deviceId, name, deviceType;
            uint32_t rawType = 0;
            uint16_t bleType = 0;
            populateDeviceFields(data.device, m_registry, deviceId, name, deviceType, rawType, bleType);

            bool paired = (data.device->state() == bluetooth::DeviceState::Paired ||
                           data.device->state() == bluetooth::DeviceState::Connected);

            if (m_callbacks.onDiscoveredDevice) {
                m_callbacks.onDiscoveredDevice(deviceId, name, deviceType, rawType, bleType,
                                               paired, false, "DISCOVERED");
            }
            // For previously known (paired) devices also emit onDeviceFound.
            if (paired && m_callbacks.onDeviceFound) {
                m_callbacks.onDeviceFound(deviceId, name, deviceType, rawType, bleType, false);
            }
            break;
        }

        case bluetooth::AdapterEvent::DeviceDisappeared: {
            if (!data.device) break;
            std::string deviceId, name, deviceType;
            uint32_t rawType = 0;
            uint16_t bleType = 0;
            populateDeviceFields(data.device, m_registry, deviceId, name, deviceType, rawType, bleType);

            bool paired = (data.device->state() == bluetooth::DeviceState::Paired ||
                           data.device->state() == bluetooth::DeviceState::Connected);

            if (m_callbacks.onDiscoveredDevice) {
                m_callbacks.onDiscoveredDevice(deviceId, name, deviceType, rawType, bleType,
                                               paired, false, "LOST");
            }
            if (paired && m_callbacks.onDeviceLost) {
                m_callbacks.onDeviceLost(deviceId, name, deviceType, rawType, bleType, false);
            }
            break;
        }
    }
}

void EventBridge::onDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;

    switch (event) {
        case bluetooth::DeviceEvent::Paired:
            emitDeviceStatusChanged("PAIRING_CHANGE", std::move(device), true, false);
            break;

        case bluetooth::DeviceEvent::Unpaired:
            emitDeviceStatusChanged("PAIRING_CHANGE", std::move(device), false, false);
            break;

        case bluetooth::DeviceEvent::Connected:
            emitDeviceStatusChanged("CONNECTION_CHANGE", std::move(device), true, true);
            break;

        case bluetooth::DeviceEvent::Disconnected:
            emitDeviceStatusChanged("CONNECTION_CHANGE", std::move(device), true, false);
            break;

        default:
            break;
    }
}

void EventBridge::emitDeviceStatusChanged(const std::string& newStatus,
                                          std::shared_ptr<bluetooth::Device> device,
                                          bool paired, bool connected) {
    if (!m_callbacks.onStatusChanged) return;

    std::string deviceId, name, deviceType;
    uint32_t rawType = 0;
    uint16_t bleType = 0;
    populateDeviceFields(std::move(device), m_registry, deviceId, name, deviceType, rawType, bleType);

    bool hasAutoConnect = false;
    bool autoConnect    = false;
    if (m_callbacks.getAutoConnect) {
        hasAutoConnect = m_callbacks.getAutoConnect(deviceId, autoConnect);
    }

    m_callbacks.onStatusChanged("onStatusChanged", newStatus,
                                deviceId, name, deviceType, rawType, bleType,
                                paired, connected, false,
                                hasAutoConnect, autoConnect);
}

} // namespace Plugin
} // namespace WPEFramework
