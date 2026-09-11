/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
*/

#include <bluetooth/Uuid.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
// Base UUID all 16-bit "well-known" Bluetooth UUIDs are derived from.
constexpr std::array<uint8_t, 16> kBluetoothBaseUuidBytes = {
    0x00, 0x00, 0x00, 0x00,             // First 4 bytes
    0x00, 0x00,                         // Next 2 bytes
    0x10, 0x00,                         // Next 2 bytes
    0x80, 0x00,                         // Next 2 bytes
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB  // Last 6 bytes
};
}  // namespace

namespace bluetooth {

Uuid::Uuid(uint16_t id) {
  std::copy(kBluetoothBaseUuidBytes.begin(), kBluetoothBaseUuidBytes.end(), m_uuid);
  m_uuid[2] = static_cast<uint8_t>((id >> 8) & 0xff);
  m_uuid[3] = static_cast<uint8_t>(id & 0xff);
}

Uuid::Uuid(const std::string& id) {
  unsigned int bytes[16];
  const int matched = std::sscanf(id.c_str(),
      "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
      &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5], &bytes[6], &bytes[7],
      &bytes[8], &bytes[9], &bytes[10], &bytes[11], &bytes[12], &bytes[13], &bytes[14], &bytes[15]);
  if (matched != 16) {
    throw std::runtime_error("Invalid 128-bit uuid:" + id);
  }
  std::transform(std::begin(bytes), std::end(bytes), m_uuid,
                 [](unsigned int b) { return static_cast<uint8_t>(b); });
}

bool Uuid::isWellKnown() const {
  return std::equal(kBluetoothBaseUuidBytes.begin() + 2, kBluetoothBaseUuidBytes.end(), m_uuid + 2);
}

std::string Uuid::toString(bool shortForm) const {
  if (isWellKnown() && shortForm) {
    const uint16_t wellKnownUuid = static_cast<uint16_t>((m_uuid[2] << 8) | m_uuid[3]);
    std::stringstream buff;
    buff << "0x" << std::hex << std::setw(4) << std::setfill('0') << wellKnownUuid;
    return buff.str();
  }

  char buff[37];
  std::snprintf(buff, sizeof(buff),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      m_uuid[0], m_uuid[1], m_uuid[2], m_uuid[3], m_uuid[4], m_uuid[5], m_uuid[6], m_uuid[7],
      m_uuid[8], m_uuid[9], m_uuid[10], m_uuid[11], m_uuid[12], m_uuid[13], m_uuid[14], m_uuid[15]);
  return std::string(buff);
}

}  // namespace bluetooth
