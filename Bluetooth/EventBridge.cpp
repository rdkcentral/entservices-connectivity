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

#include <core/core.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <UtilsLogging.h>

#include "DeviceDisplayName.h"

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

    outName = deviceDisplayName(device);

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

EventBridge::~EventBridge() {
    stop();
}

// Pending notifications are dropped rather than flushed: they hold SDK Device objects and call back into the
// plugin, neither of which may be used once the adapter is being torn down.
void EventBridge::stop() {
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_stopping = true;
        m_tasks.clear();
    }
    m_taskCv.notify_all();
    if (m_worker.joinable()) m_worker.join();
    std::lock_guard<std::mutex> lock(m_opsMutex);
    m_ops.clear();
}

void EventBridge::enqueue(std::chrono::steady_clock::time_point due, std::function<void()> run) {
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        if (m_stopping) return;
        m_tasks.push_back({due, std::move(run)});
        // Started on first use so an EventBridge that never sees a device event costs no thread.
        if (!m_worker.joinable()) m_worker = std::thread(&EventBridge::worker, this);
    }
    m_taskCv.notify_all();
}

void EventBridge::worker() {
    std::unique_lock<std::mutex> lock(m_taskMutex);
    for (;;) {
        m_taskCv.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
        if (m_stopping) return;
        // Tasks run strictly in order, so a delayed task also delays the ones queued behind it. That is
        // intentional: anything queued after a request completed must not be delivered before that request's
        // held status changes.
        const auto due = m_tasks.front().due;
        if (m_taskCv.wait_until(lock, due, [this] { return m_stopping; })) return;
        std::function<void()> run = std::move(m_tasks.front().run);
        m_tasks.pop_front();
        // Run unlocked: building a notification makes D-Bus calls and the plugin callback may take its own
        // locks, and the SDK event thread must stay free to enqueue meanwhile.
        lock.unlock();
        run();
        lock.lock();
    }
}

void EventBridge::beginDeviceOperation(const std::shared_ptr<bluetooth::Device>& device) {
    if (!device) return;
    std::string mac;
    device->address(mac);
    std::lock_guard<std::mutex> lock(m_opsMutex);
    DeviceOperation& op = m_ops[mac];
    // active is a count because a new request may begin while the previous one is still awaiting its delayed
    // release; the entry, and with it the holding of status changes, lasts until both have been released.
    ++op.active;
    // Only status changes raised by this request decide whether its completion event must be synthesized.
    op.seen = 0;
}

void EventBridge::completeDeviceOperation(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device,
                                          bool succeeded) {
    if (!device) return;
    std::string mac;
    device->address(mac);
    // Status changes keep being held until the release runs, so ones the SDK raises just after the request
    // returned (it can return before raising them) are still delivered after the response, not ahead of it.
    enqueue(std::chrono::steady_clock::now() + kCompletionDelay,
            [this, mac, event, device = std::move(device), succeeded]() mutable {
                releaseDeviceOperation(mac, event, std::move(device), succeeded);
            });
}

void EventBridge::releaseDeviceOperation(const std::string& mac, bluetooth::DeviceEvent event,
                                         std::shared_ptr<bluetooth::Device> device, bool succeeded) {
    std::vector<HeldEvent> held;
    bool seen = false;
    {
        std::lock_guard<std::mutex> lock(m_opsMutex);
        auto it = m_ops.find(mac);
        if (it == m_ops.end()) return;
        held = std::move(it->second.held);
        it->second.held.clear();
        seen = (it->second.seen & (1u << static_cast<unsigned>(event))) != 0;
        if (--it->second.active == 0) m_ops.erase(it);
    }
    // Held changes are emitted in the order the SDK raised them, e.g. Connected then Paired while pairing.
    for (auto& h : held) emitDeviceEvent(h.event, std::move(h.device));
    // A successful request must always be acknowledged, as the legacy stack did. The SDK raises nothing when
    // there was no state change, e.g. connecting a device BlueZ already connected while pairing it.
    if (succeeded && !seen) {
        LOGINFO("EventBridge::releaseDeviceOperation: SDK reported no event for event=%d mac=%s, emitting it",
                static_cast<int>(event), mac.c_str());
        emitDeviceEvent(event, std::move(device));
    }
}

void EventBridge::onDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;
    std::string mac;
    device->address(mac);
    {
        std::lock_guard<std::mutex> lock(m_opsMutex);
        auto it = m_ops.find(mac);
        if (it != m_ops.end()) {
            // A pair/connect request for this device is in progress; hold the change until its response is out.
            it->second.seen |= 1u << static_cast<unsigned>(event);
            it->second.held.push_back({event, std::move(device)});
            return;
        }
    }
    // Unsolicited changes (e.g. the device dropping the link) are due now, but still go through the queue so they
    // stay behind any pending release of earlier, held changes.
    enqueue(std::chrono::steady_clock::now(),
            [this, event, device = std::move(device)]() mutable { emitDeviceEvent(event, std::move(device)); });
}

void EventBridge::emitDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device) {
    switch (event) {
        case bluetooth::DeviceEvent::Paired: {
            // BlueZ may already have connected the device while pairing (e.g. headphones).
            const bool connected = (device->state() == bluetooth::DeviceState::Connected);
            emitDeviceStatusChanged("PAIRING_CHANGE", std::move(device), true, connected);
            break;
        }

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
