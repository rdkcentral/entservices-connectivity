/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
*/

/**
 * @file Device.cpp
 * @brief Fake Device backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/Device.h>

#include <bluetooth/Adapter.h>
#include <bluetooth/GattClient.h>

#include <functional>
#include <utility>

namespace bluetooth {

namespace {
Status unavailable() { return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available"); }
}  // namespace

class Device::Impl {
 public:
  Impl(Device* owner, std::weak_ptr<bluetooth::Adapter> adapter, std::string objectPath)
      : m_owner(owner), m_adapter(std::move(adapter)), m_address(std::move(objectPath)) {
    m_gattClient = std::make_shared<gattClient::Client>(m_address);
  }

  Status address(std::string& str) {
    str = m_address;
    return Status();
  }
  Status name(std::string& str) {
    str.clear();
    return unavailable();
  }
  Status connected(bool& value) {
    value = (m_state == DeviceState::Connected);
    return Status();
  }
  Status rssi(int8_t& rssi) {
    rssi = 0;
    return unavailable();
  }
  Status manufacturerData(std::map<uint16_t, std::vector<uint8_t>>&) { return unavailable(); }
  Status getAllProperties(DeviceProperties& properties) {
    properties = DeviceProperties();
    properties.address = m_address;
    return Status();
  }
  DeviceState state() { return m_state; }
  void state(DeviceState state) { m_state = state; }
  Status connect(bool, uint8_t) { return unavailable(); }
  Status pair(bool, uint8_t) { return unavailable(); }
  Status disconnect(bool, uint8_t) { return unavailable(); }
  Status unpair() { return unavailable(); }
  std::shared_ptr<gattClient::Client> getGattClient() { return m_gattClient; }
  Status setAutoconnectOn() { return unavailable(); }
  Status setAutoconnectOff() { return unavailable(); }
  Status getBatteryLevel(uint8_t& percentage) {
    percentage = 0;
    return unavailable();
  }
  Status signalStrength(SignalStrength& strength) {
    strength = SignalStrength::None;
    return unavailable();
  }
  bool isInCooldown() const { return false; }

  Device* m_owner;
  std::weak_ptr<bluetooth::Adapter> m_adapter;

 private:
  std::shared_ptr<gattClient::Client> m_gattClient;
  DeviceState m_state{DeviceState::Discovered};
  std::string m_address;
};

Device::Device(std::weak_ptr<bluetooth::Adapter> adapter, const std::string& objectPath)
    : m_impl(std::make_unique<Impl>(this, std::move(adapter), objectPath)) {}

Device::Device(std::weak_ptr<bluetooth::Adapter> adapter, const std::string& objectPath,
               const std::vector<std::string>& /* managedObjectPaths */)
    : m_impl(std::make_unique<Impl>(this, std::move(adapter), objectPath)) {}

Device::~Device() = default;

Status Device::address(std::string& str) { return m_impl->address(str); }
Status Device::name(std::string& str) { return m_impl->name(str); }
Status Device::connected(bool& value) { return m_impl->connected(value); }
Status Device::rssi(int8_t& rssi) { return m_impl->rssi(rssi); }
Status Device::manufacturerData(std::map<uint16_t, std::vector<uint8_t>>& data) { return m_impl->manufacturerData(data); }
Status Device::getAllProperties(DeviceProperties& properties) { return m_impl->getAllProperties(properties); }
DeviceState Device::state() { return m_impl->state(); }
void Device::state(DeviceState s) { m_impl->state(s); }
Status Device::connect(bool sync, uint8_t timeout) { return m_impl->connect(sync, timeout); }
Status Device::pair(bool sync, uint8_t timeout) { return m_impl->pair(sync, timeout); }
Status Device::disconnect(bool sync, uint8_t timeout) { return m_impl->disconnect(sync, timeout); }
Status Device::unpair() { return m_impl->unpair(); }
std::shared_ptr<gattClient::Client> Device::getGattClient() { return m_impl->getGattClient(); }
Status Device::setAutoconnectOn() { return m_impl->setAutoconnectOn(); }
Status Device::setAutoconnectOff() { return m_impl->setAutoconnectOff(); }
Status Device::getBatteryLevel(uint8_t& percentage) { return m_impl->getBatteryLevel(percentage); }
Status Device::signalStrength(SignalStrength& strength) { return m_impl->signalStrength(strength); }
bool Device::isInCooldown() const { return m_impl->isInCooldown(); }

Status Device::adapterRemove(const std::string& devicePath) {
  if (auto a = m_impl->m_adapter.lock()) return a->remove(devicePath);
  return Status(StatusCodes::BLUETOOTH_ERROR, "Adapter not available");
}

Status Device::adapterSetPairable(bool pairable) {
  if (auto a = m_impl->m_adapter.lock()) return a->setPairable(pairable);
  return Status(StatusCodes::BLUETOOTH_ERROR, "Adapter not available");
}

#ifdef AUDIO_SUPPORT
Status Device::setVolume(float) { return unavailable(); }
float Device::getVolume() { return 0.0f; }
Status Device::setMute(bool) { return unavailable(); }
bool Device::isMuted() { return false; }
Status Device::setDelayCompensation(uint32_t) { return unavailable(); }
void Device::registerForAudioEvents(std::function<void(AudioEvent, AudioEventData)>) {}
Status Device::initAudio(WpNode*, WpProxy*) { return unavailable(); }
#endif

}  // namespace bluetooth
