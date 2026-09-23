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
 * @file Advertisement.cpp
 * @brief Fake Advertisement backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/Advertisement.h>

#include <utility>

namespace bluetooth {

class Advertisement::Impl {
 public:
  void Type(AdvertisementType type) { m_type = type; }
  void ServiceUUIDs(std::vector<Uuid> serviceUuids) { m_serviceUuids = std::move(serviceUuids); }
  void ManufacturerData(std::map<uint16_t, std::vector<uint8_t>>& data) { m_manufacturerData = data; }
  void ServiceData(std::vector<Uuid>& serviceUuids) { m_serviceUuids = serviceUuids; }
  void DiscoverableTimeout(std::chrono::seconds value) { m_discoverableTimeout = value; }
  void LocalName(std::string name) { m_localName = std::move(name); }
  void AppearanceValue(Appearance appearance) { m_appearance = appearance; }
  void Duration(std::chrono::seconds duration) { m_duration = duration; }
  void Timeout(std::chrono::seconds timeout) { m_timeout = timeout; }
  void MinInterval(std::chrono::milliseconds interval) { m_minInterval = interval; }
  void MaxInterval(std::chrono::milliseconds interval) { m_maxInterval = interval; }
  void TxPower(int16_t txPower) { m_txPower = txPower; }
  void registered(bool registered) { m_registered = registered; }

 private:
  AdvertisementType m_type{AdvertisementType::Peripheral};
  std::vector<Uuid> m_serviceUuids;
  std::map<uint16_t, std::vector<uint8_t>> m_manufacturerData;
  std::chrono::seconds m_discoverableTimeout{0};
  std::string m_localName{"Bluetooth SDK"};
  Appearance m_appearance{0};
  std::chrono::seconds m_duration{2};
  std::chrono::seconds m_timeout{0};
  std::chrono::milliseconds m_minInterval{25};
  std::chrono::milliseconds m_maxInterval{1000};
  int16_t m_txPower{0};
  bool m_registered{false};
};

Advertisement::Advertisement() : m_impl(std::make_unique<Impl>()) {}
Advertisement::~Advertisement() = default;

void Advertisement::Type(AdvertisementType type) { m_impl->Type(type); }
void Advertisement::ServiceUUIDs(std::vector<Uuid> serviceUuids) { m_impl->ServiceUUIDs(std::move(serviceUuids)); }
void Advertisement::ManufacturerData(std::map<uint16_t, std::vector<uint8_t>>& data) { m_impl->ManufacturerData(data); }
void Advertisement::ServiceData(std::vector<Uuid>& serviceUuids) { m_impl->ServiceData(serviceUuids); }
void Advertisement::DiscoverableTimeout(std::chrono::seconds value) { m_impl->DiscoverableTimeout(value); }
void Advertisement::LocalName(std::string name) { m_impl->LocalName(std::move(name)); }
void Advertisement::AppearanceValue(Appearance appearance) { m_impl->AppearanceValue(appearance); }
void Advertisement::Duration(std::chrono::seconds duration) { m_impl->Duration(duration); }
void Advertisement::Timeout(std::chrono::seconds timeout) { m_impl->Timeout(timeout); }
void Advertisement::MinInterval(std::chrono::milliseconds interval) { m_impl->MinInterval(interval); }
void Advertisement::MaxInterval(std::chrono::milliseconds interval) { m_impl->MaxInterval(interval); }
void Advertisement::TxPower(int16_t txPower) { m_impl->TxPower(txPower); }
void Advertisement::registered(bool registered) { m_impl->registered(registered); }

class AdvertisementMgr::Impl {
 public:
  explicit Impl(std::string adapterPath) : m_adapterPath(std::move(adapterPath)) {}

  Status startAdvertising(std::shared_ptr<Advertisement> advertisement) {
    m_advertisement = std::move(advertisement);
    return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available");
  }
  Status stopAdvertising() {
    m_advertisement = nullptr;
    return Status();
  }

 private:
  std::string m_adapterPath;
  std::shared_ptr<Advertisement> m_advertisement;
};

AdvertisementMgr::AdvertisementMgr(const std::string& adapterPath) : m_impl(std::make_unique<Impl>(adapterPath)) {}
AdvertisementMgr::~AdvertisementMgr() = default;

Status AdvertisementMgr::startAdvertising(std::shared_ptr<Advertisement> advertisement) {
  return m_impl->startAdvertising(std::move(advertisement));
}
Status AdvertisementMgr::stopAdvertising() { return m_impl->stopAdvertising(); }

}  // namespace bluetooth
