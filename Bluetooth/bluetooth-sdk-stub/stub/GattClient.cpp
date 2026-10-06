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
 * @file GattClient.cpp
 * @brief Fake GATT client backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/GattClient.h>

#include <utility>

namespace bluetooth {

namespace {
Status unavailable() { return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available"); }

std::string extractServicePath(const std::string& characteristicPath) {
  size_t pos = characteristicPath.find("/char");
  return pos != std::string::npos ? characteristicPath.substr(0, pos) : "";
}

std::string extractCharacteristicPath(const std::string& descriptorPath) {
  size_t pos = descriptorPath.find("/desc");
  return pos != std::string::npos ? descriptorPath.substr(0, pos) : "";
}
}  // namespace

namespace gattClient {

// Descriptor implementation

class Descriptor::Impl {
 public:
  explicit Impl(std::string path) : m_path(std::move(path)) {}
  std::string UUID() { return ""; }
  std::vector<uint8_t> Value() { return {}; }
  Status ReadValue(std::vector<uint8_t>&) { return unavailable(); }
  Status WriteValue(const std::vector<uint8_t>&) { return unavailable(); }
  std::shared_ptr<gattClient::Characteristic> Characteristic() { return nullptr; }

 private:
  std::string m_path;
};

Descriptor::Descriptor(const std::string& objectPath) : m_impl(std::make_unique<Impl>(objectPath)) {}
Descriptor::~Descriptor() = default;
std::string Descriptor::UUID() { return m_impl->UUID(); }
std::vector<uint8_t> Descriptor::Value() { return m_impl->Value(); }
Status Descriptor::ReadValue(std::vector<uint8_t>& value) { return m_impl->ReadValue(value); }
Status Descriptor::WriteValue(const std::vector<uint8_t>& value) { return m_impl->WriteValue(value); }
std::shared_ptr<gattClient::Characteristic> Descriptor::Characteristic() { return m_impl->Characteristic(); }

// Characteristic implementation

class Characteristic::Impl {
 public:
  explicit Impl(std::string path) : m_path(std::move(path)), m_servicePath(extractServicePath(m_path)) {}

  std::string UUID() { return ""; }
  std::vector<uint8_t> Value() { return {}; }
  Status readValue(std::vector<uint8_t>&) { return unavailable(); }
  Status writeValue(const std::vector<uint8_t>&) { return unavailable(); }
  Status StartNotify(std::function<void(std::vector<uint8_t>)>) { return unavailable(); }
  Status StopNotify() { return unavailable(); }
  std::string Service() { return m_servicePath; }
  bool isNotifyEnable() { return false; }
  std::vector<std::string> Flags() { return {}; }

  bool addDescriptor(const std::string& descriptorPath) {
    if (descriptorPath.empty()) return false;
    if (m_descriptors.find(descriptorPath) != m_descriptors.end()) return true;
    m_descriptors[descriptorPath] = std::make_shared<Descriptor>(descriptorPath);
    return true;
  }
  std::shared_ptr<Descriptor> getDescriptor(const std::string& descriptorPath) {
    auto it = m_descriptors.find(descriptorPath);
    return it != m_descriptors.end() ? it->second : nullptr;
  }
  const std::map<std::string, std::shared_ptr<Descriptor>>& getDescriptors() const { return m_descriptors; }

 private:
  std::string m_path;
  std::string m_servicePath;
  std::map<std::string, std::shared_ptr<Descriptor>> m_descriptors;
};

Characteristic::Characteristic(const std::string& objectPath) : m_impl(std::make_unique<Impl>(objectPath)) {}
Characteristic::~Characteristic() = default;
std::string Characteristic::UUID() { return m_impl->UUID(); }
std::vector<uint8_t> Characteristic::Value() { return m_impl->Value(); }
Status Characteristic::readValue(std::vector<uint8_t>& value) { return m_impl->readValue(value); }
Status Characteristic::writeValue(const std::vector<uint8_t>& value) { return m_impl->writeValue(value); }
Status Characteristic::StartNotify(std::function<void(std::vector<uint8_t>)> notifyCb) {
  return m_impl->StartNotify(std::move(notifyCb));
}
Status Characteristic::StopNotify() { return m_impl->StopNotify(); }
std::string Characteristic::Service() { return m_impl->Service(); }
bool Characteristic::isNotifyEnable() { return m_impl->isNotifyEnable(); }
std::vector<std::string> Characteristic::Flags() { return m_impl->Flags(); }
bool Characteristic::addDescriptor(const std::string& descriptorPath) { return m_impl->addDescriptor(descriptorPath); }
std::shared_ptr<Descriptor> Characteristic::getDescriptor(const std::string& descriptorPath) {
  return m_impl->getDescriptor(descriptorPath);
}
const std::map<std::string, std::shared_ptr<Descriptor>>& Characteristic::getDescriptors() const {
  return m_impl->getDescriptors();
}

// Service implementation

class Service::Impl {
 public:
  explicit Impl(std::string path) : m_path(std::move(path)) {}

  std::string UUID() { return ""; }
  std::string Device() { return ""; }
  bool Primary() { return true; }

  bool addCharacteristic(const std::string& characteristicPath) {
    if (characteristicPath.empty()) return false;
    if (m_characteristics.find(characteristicPath) != m_characteristics.end()) return true;
    m_characteristics[characteristicPath] = std::make_shared<Characteristic>(characteristicPath);
    return true;
  }
  std::shared_ptr<Characteristic> getCharacteristic(const std::string& characteristicPath) {
    auto it = m_characteristics.find(characteristicPath);
    return it != m_characteristics.end() ? it->second : nullptr;
  }
  const std::map<std::string, std::shared_ptr<Characteristic>>& getCharacteristics() const { return m_characteristics; }

 private:
  std::string m_path;
  std::map<std::string, std::shared_ptr<Characteristic>> m_characteristics;
};

Service::Service(const std::string& objectPath) : m_impl(std::make_unique<Impl>(objectPath)) {}
Service::~Service() = default;
std::string Service::UUID() { return m_impl->UUID(); }
std::string Service::Device() { return m_impl->Device(); }
bool Service::Primary() { return m_impl->Primary(); }
bool Service::addCharacteristic(const std::string& characteristicPath) {
  return m_impl->addCharacteristic(characteristicPath);
}
std::shared_ptr<Characteristic> Service::getCharacteristic(const std::string& characteristicPath) {
  return m_impl->getCharacteristic(characteristicPath);
}
const std::map<std::string, std::shared_ptr<Characteristic>>& Service::getCharacteristics() const {
  return m_impl->getCharacteristics();
}

// Client implementation (already sdbus-free; identical to the real backend)

Client::Client(const std::string& devicePath) : m_devicePath(devicePath) {}

Client::~Client() {}

std::shared_ptr<Service> Client::getService(const std::string& servicePath) {
  auto it = m_services.find(servicePath);
  if (it != m_services.end()) {
    return it->second;
  }
  return nullptr;
}

const std::map<std::string, std::shared_ptr<Service>>& Client::getServices() { return m_services; }

std::shared_ptr<Service> Client::GetServiceByUUID(const std::string& uuid) {
  for (const auto& servicePair : m_services) {
    if (servicePair.second->UUID() == uuid) {
      return servicePair.second;
    }
  }
  return nullptr;
}

std::shared_ptr<Characteristic> Client::GetCharacteristicByUUID(const std::string& uuid) {
  for (const auto& servicePair : m_services) {
    for (const auto& charPair : servicePair.second->getCharacteristics()) {
      if (charPair.second->UUID() == uuid) {
        return charPair.second;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<Descriptor> Client::GetDescriptorByUUID(const std::string& uuid) {
  for (const auto& servicePair : m_services) {
    for (const auto& charPair : servicePair.second->getCharacteristics()) {
      for (const auto& desPair : charPair.second->getDescriptors()) {
        if (desPair.second->UUID() == uuid) {
          return desPair.second;
        }
      }
    }
  }
  return nullptr;
}

bool Client::addService(const std::string& servicePath) {
  if (servicePath.empty()) {
    return false;
  }
  if (m_services.find(servicePath) == m_services.end()) {
    m_services[servicePath] = std::make_shared<Service>(servicePath);
  }
  return true;
}

bool Client::addCharacteristic(const std::string& characteristicPath) {
  std::string servicePath = extractServicePath(characteristicPath);
  if (servicePath.empty()) {
    return false;
  }

  auto service = getService(servicePath);
  if (!service && !addService(servicePath)) {
    return false;
  }
  service = getService(servicePath);
  return service->addCharacteristic(characteristicPath);
}

bool Client::addDescriptor(const std::string& descriptorPath) {
  std::string characteristicPath = extractCharacteristicPath(descriptorPath);
  if (characteristicPath.empty()) {
    return false;
  }

  if (!addCharacteristic(characteristicPath)) {
    return false;
  }
  std::string servicePath = extractServicePath(characteristicPath);
  auto service = getService(servicePath);
  auto characteristic = service->getCharacteristic(characteristicPath);
  return characteristic->addDescriptor(descriptorPath);
}

}  // namespace gattClient
}  // namespace bluetooth
