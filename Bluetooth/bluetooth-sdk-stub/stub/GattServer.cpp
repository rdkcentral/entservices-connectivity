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
 * @file GattServer.cpp
 * @brief Fake GATT server backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/GattServer.h>

#include <algorithm>
#include <utility>

namespace bluetooth {

namespace {
Status unavailable() { return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available"); }

class PathGenerator {
 public:
  static PathGenerator& getGenerator() {
    static PathGenerator gen;
    return gen;
  }
  std::string nextDescriptorPath() { return "/stub/gatt/descriptor" + std::to_string(m_descriptorIndex++); }
  std::string nextCharacteristicPath() { return "/stub/gatt/characteristic" + std::to_string(m_characteristicIndex++); }
  std::string nextServicePath() { return "/stub/gatt/service" + std::to_string(m_serviceIndex++); }

 private:
  PathGenerator() = default;
  uint16_t m_descriptorIndex = 1;
  uint16_t m_characteristicIndex = 1;
  uint16_t m_serviceIndex = 1;
};
}  // namespace

namespace gattServer {

// Descriptor implementation

class Descriptor::Impl {
 public:
  Impl(std::string uuid, std::vector<uint8_t> value, uint16_t /* handle */)
      : m_uuid(std::move(uuid)), m_currentValue(std::move(value)) {}
  Impl(std::string uuid, std::function<std::vector<uint8_t>()> readCb,
       std::function<void(std::vector<uint8_t>)> writeCb, uint16_t /* handle */)
      : m_uuid(std::move(uuid)), m_readCb(std::move(readCb)), m_writeCb(std::move(writeCb)) {}

  std::string UUID() { return m_uuid; }
  std::vector<uint8_t> Value() { return m_readCb ? m_readCb() : m_currentValue; }
  void setValue(std::vector<uint8_t> value) {
    if (m_writeCb) {
      m_writeCb(std::move(value));
    } else {
      m_currentValue = std::move(value);
    }
  }
  void setCharacteristic(std::shared_ptr<gattServer::Characteristic> characteristic) {
    m_characteristic = std::move(characteristic);
  }

 private:
  std::string m_uuid;
  std::function<std::vector<uint8_t>()> m_readCb;
  std::function<void(std::vector<uint8_t>)> m_writeCb;
  std::vector<uint8_t> m_currentValue;
  std::shared_ptr<gattServer::Characteristic> m_characteristic;
};

Descriptor::Descriptor(std::string UUID, std::vector<uint8_t> value, uint16_t handle)
    : m_path(PathGenerator::getGenerator().nextDescriptorPath()),
      m_impl(std::make_unique<Impl>(std::move(UUID), std::move(value), handle)) {}

Descriptor::Descriptor(std::string UUID, std::function<std::vector<uint8_t>()> readCb,
                       std::function<void(std::vector<uint8_t>)> writeCb, uint16_t handle)
    : m_path(PathGenerator::getGenerator().nextDescriptorPath()),
      m_impl(std::make_unique<Impl>(std::move(UUID), std::move(readCb), std::move(writeCb), handle)) {}

Descriptor::~Descriptor() = default;

std::string Descriptor::UUID() { return m_impl->UUID(); }
std::vector<uint8_t> Descriptor::Value() { return m_impl->Value(); }
void Descriptor::setValue(std::vector<uint8_t> value) { m_impl->setValue(std::move(value)); }
void Descriptor::Characteristic(std::shared_ptr<gattServer::Characteristic> characteristic) {
  m_impl->setCharacteristic(std::move(characteristic));
}

// Characteristic implementation

class Characteristic::Impl {
 public:
  Impl(std::string uuid, std::vector<uint8_t> value, GattProperties properties, uint16_t mtu)
      : m_uuid(std::move(uuid)), m_currentValue(std::move(value)), m_gattProperties(properties), m_mtu(mtu) {}
  Impl(std::string uuid, std::function<std::vector<uint8_t>()> readCb,
       std::function<void(std::vector<uint8_t>)> writeCb, GattProperties properties, uint16_t mtu)
      : m_uuid(std::move(uuid)),
        m_readCb(std::move(readCb)),
        m_writeCb(std::move(writeCb)),
        m_gattProperties(properties),
        m_mtu(mtu) {}

  std::string UUID() { return m_uuid; }
  std::vector<uint8_t> Value() { return m_readCb ? m_readCb() : m_currentValue; }
  void setValue(std::vector<uint8_t> value) {
    if (m_writeCb) {
      m_writeCb(std::move(value));
    } else {
      m_currentValue = std::move(value);
    }
  }
  void setService(std::shared_ptr<gattServer::Service> service) { m_service = std::move(service); }

  bool addDescriptor(const std::shared_ptr<Descriptor>& descriptor, const std::shared_ptr<Characteristic>& self) {
    if (!descriptor) return false;
    descriptor->Characteristic(self);
    m_descriptors[descriptor->m_path] = descriptor;
    return true;
  }
  const std::map<std::string, std::shared_ptr<Descriptor>>& getDescriptors() const { return m_descriptors; }

 private:
  std::string m_uuid;
  std::function<std::vector<uint8_t>()> m_readCb;
  std::function<void(std::vector<uint8_t>)> m_writeCb;
  std::vector<uint8_t> m_currentValue;
  std::shared_ptr<gattServer::Service> m_service;
  std::map<std::string, std::shared_ptr<Descriptor>> m_descriptors;
  GattProperties m_gattProperties;
  uint16_t m_mtu;
};

Characteristic::Characteristic(std::string UUID, GattProperties properties, std::vector<uint8_t> value,
                               uint16_t handle, uint16_t mtu)
    : m_path(PathGenerator::getGenerator().nextCharacteristicPath()),
      m_impl(std::make_unique<Impl>(std::move(UUID), std::move(value), properties, mtu)) {
  (void)handle;
}

Characteristic::Characteristic(std::string UUID, GattProperties properties,
                               std::function<std::vector<uint8_t>()> readCb,
                               std::function<void(std::vector<uint8_t>)> writeCb, uint16_t handle, uint16_t mtu)
    : m_path(PathGenerator::getGenerator().nextCharacteristicPath()),
      m_impl(std::make_unique<Impl>(std::move(UUID), std::move(readCb), std::move(writeCb), properties, mtu)) {
  (void)handle;
}

Characteristic::~Characteristic() = default;

std::string Characteristic::UUID() { return m_impl->UUID(); }
std::vector<uint8_t> Characteristic::Value() { return m_impl->Value(); }
void Characteristic::setValue(std::vector<uint8_t> value) { m_impl->setValue(std::move(value)); }
void Characteristic::Service(std::shared_ptr<gattServer::Service> service) { m_impl->setService(std::move(service)); }
bool Characteristic::addDescriptor(std::shared_ptr<Descriptor> descriptor) {
  return m_impl->addDescriptor(descriptor, shared_from_this());
}
const std::map<std::string, std::shared_ptr<Descriptor>>& Characteristic::getDescriptors() const {
  return m_impl->getDescriptors();
}

// Service implementation

class Service::Impl {
 public:
  Impl(std::string uuid, bool primary, std::vector<std::shared_ptr<Service>> includes)
      : m_uuid(std::move(uuid)), m_primary(primary), m_includes(std::move(includes)) {}

  std::string UUID() { return m_uuid; }

  bool addCharacteristic(const std::shared_ptr<Characteristic>& characteristic, const std::shared_ptr<Service>& self) {
    if (!characteristic) return false;
    characteristic->Service(self);
    m_characteristics[characteristic->m_path] = characteristic;
    return true;
  }
  const std::map<std::string, std::shared_ptr<Characteristic>>& getCharacteristics() const { return m_characteristics; }

 private:
  std::string m_uuid;
  bool m_primary;
  std::vector<std::shared_ptr<Service>> m_includes;
  std::map<std::string, std::shared_ptr<Characteristic>> m_characteristics;
};

Service::Service(std::string UUID, bool primary, uint16_t handle, std::vector<std::shared_ptr<Service>> includes)
    : m_path(PathGenerator::getGenerator().nextServicePath()),
      m_impl(std::make_unique<Impl>(std::move(UUID), primary, std::move(includes))) {
  (void)handle;
}

Service::~Service() = default;

std::string Service::UUID() { return m_impl->UUID(); }
bool Service::addCharacteristic(std::shared_ptr<Characteristic> characteristic) {
  return m_impl->addCharacteristic(characteristic, shared_from_this());
}
const std::map<std::string, std::shared_ptr<Characteristic>>& Service::getCharacteristics() const {
  return m_impl->getCharacteristics();
}

// Server implementation

class Server::Impl {
 public:
  Impl(std::shared_ptr<Adapter> adapter, uint8_t connections) : m_adapter(std::move(adapter)), m_maxConnections(connections) {}

  bool addService(std::shared_ptr<Service> service) {
    if (!service) return false;
    m_services.push_back(std::move(service));
    return true;
  }
  bool removeService(const std::shared_ptr<Service>& service) {
    auto it = std::find(m_services.begin(), m_services.end(), service);
    if (it == m_services.end()) return false;
    m_services.erase(it);
    return true;
  }
  Status start() { return unavailable(); }
  Status stop() {
    m_running = false;
    return Status();
  }

 private:
  std::shared_ptr<Adapter> m_adapter;
  std::vector<std::shared_ptr<Service>> m_services;
  int m_maxConnections;
  bool m_running = false;
};

Server::Server(std::shared_ptr<Adapter> adapter, uint8_t connections)
    : m_impl(std::make_unique<Impl>(std::move(adapter), connections)) {}
Server::~Server() = default;

bool Server::addService(std::shared_ptr<Service> service) { return m_impl->addService(std::move(service)); }
bool Server::removeService(std::shared_ptr<Service> service) { return m_impl->removeService(service); }
Status Server::start() { return m_impl->start(); }
Status Server::stop() { return m_impl->stop(); }

}  // namespace gattServer
}  // namespace bluetooth
