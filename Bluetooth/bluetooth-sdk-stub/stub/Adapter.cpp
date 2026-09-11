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
 * @file Adapter.cpp
 * @brief Fake Adapter backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/Adapter.h>

#include <utility>

namespace bluetooth {

namespace {
Status unavailable() { return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available"); }
}  // namespace

class Adapter::Impl {
 public:
  Impl(Adapter* owner, std::string path) : m_owner(owner), m_path(std::move(path)) {}

  Status startScan(ScanFilter) { return unavailable(); }
  Status stopScan() { return unavailable(); }
  std::vector<std::shared_ptr<Device>> getDevices() { return {}; }
  std::vector<std::shared_ptr<Device>> getDevices(DeviceState) { return {}; }
  Status setName(const std::string&) { return unavailable(); }
  std::shared_ptr<Device> getDevice(const std::string&) { return nullptr; }
  void pruneDiscoveredDevices(const std::shared_ptr<Device>&, unsigned int) {}
  Status getPowered(bool& powered) {
    powered = false;
    return unavailable();
  }
  Status setPowered(bool) { return unavailable(); }
  Status setPairable(bool) { return unavailable(); }
  Status remove(const std::string&) { return unavailable(); }
  std::shared_ptr<AdvertisementMgr> getAdvertisementMgr() { return nullptr; }
  std::shared_ptr<gattServer::Server> getGattServerMgr(int) { return nullptr; }
  void addDevices(const std::map<std::string, std::vector<std::string>>&) {}
  void newDeviceFound(const std::string&) {}
  void deviceRemoved(const std::string&) {}
  std::string path() const { return m_path; }

 private:
  Adapter* m_owner;
  std::string m_path;
};

Adapter::Adapter(int id) : m_impl(std::make_unique<Impl>(this, "/stub/hci" + std::to_string(id))) {}
Adapter::Adapter(std::string adapterPath) : m_impl(std::make_unique<Impl>(this, std::move(adapterPath))) {}
Adapter::~Adapter() = default;

Status Adapter::startScan(ScanFilter filter) { return m_impl->startScan(std::move(filter)); }
Status Adapter::stopScan() { return m_impl->stopScan(); }
std::vector<std::shared_ptr<Device>> Adapter::getDevices() { return m_impl->getDevices(); }
Status Adapter::setName(const std::string& name) { return m_impl->setName(name); }
std::vector<std::shared_ptr<Device>> Adapter::getDevices(DeviceState state) { return m_impl->getDevices(state); }
std::shared_ptr<Device> Adapter::getDevice(const std::string& macAddress) { return m_impl->getDevice(macAddress); }
void Adapter::pruneDiscoveredDevices(const std::shared_ptr<Device>& keep, unsigned int graceSeconds) {
  m_impl->pruneDiscoveredDevices(keep, graceSeconds);
}
Status Adapter::getPowered(bool& powered) { return m_impl->getPowered(powered); }
Status Adapter::setPowered(bool powered) { return m_impl->setPowered(powered); }
std::shared_ptr<AdvertisementMgr> Adapter::getAdvertisementMgr() { return m_impl->getAdvertisementMgr(); }
std::shared_ptr<gattServer::Server> Adapter::getGattServerMgr(int connections) {
  return m_impl->getGattServerMgr(connections);
}

Status Adapter::setPairable(bool pairable) { return m_impl->setPairable(pairable); }
Status Adapter::remove(const std::string& devicePath) { return m_impl->remove(devicePath); }
std::string Adapter::path() const { return m_impl->path(); }
void Adapter::addDevices(const std::map<std::string, std::vector<std::string>>& managedDevices) {
  m_impl->addDevices(managedDevices);
}
void Adapter::newDeviceFound(const std::string& devicePath) { m_impl->newDeviceFound(devicePath); }
void Adapter::deviceRemoved(const std::string& devicePath) { m_impl->deviceRemoved(devicePath); }

#ifdef AUDIO_SUPPORT
WpNode* Adapter::findWirePlumberAudioNode(const std::string&) { return nullptr; }
#endif

}  // namespace bluetooth
