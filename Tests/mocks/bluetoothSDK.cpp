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

// Stub implementations for bluetooth-sdk types — used in SDK L2 test builds.
//
// bluetooth::Manager delegates all calls through g_managerStub (set by the
// test fixture).  bluetooth::Uuid provides the static ServiceClasses constants.
// This file is compiled into WPEFrameworkBluetooth in place of the real SDK.
//
// Note: WPEFrameworkBluetooth does NOT link librdk_bluetooth in test builds
// (see Bluetooth/CMakeLists.txt), so any bluetooth-sdk-stub symbol used by
// plugin code (e.g. BtSdkAdapterImpl.cpp's bluetooth::Uuid(uint16_t) calls)
// must be defined here too, or it is left undefined until runtime dlopen.

#include <algorithm>
#include <array>

#include <bluetooth/Manager.h>
#include <bluetooth/Uuid.h>

namespace bluetooth {

// Base UUID all 16-bit "well-known" Bluetooth UUIDs are derived from
// (mirrors bluetooth-sdk-stub/stub/Uuid.cpp).
namespace {
constexpr std::array<uint8_t, 16> kBluetoothBaseUuidBytes = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
}  // namespace

Uuid::Uuid(uint16_t id) {
    std::copy(kBluetoothBaseUuidBytes.begin(), kBluetoothBaseUuidBytes.end(), m_uuid);
    m_uuid[2] = static_cast<uint8_t>((id >> 8) & 0xff);
    m_uuid[3] = static_cast<uint8_t>(id & 0xff);
}

// ── Manager stub ─────────────────────────────────────────────────────────────

IManagerStub* g_managerStub = nullptr;

Manager::Manager(AuthorisationMode, std::function<bool(AuthorisationType, std::shared_ptr<Device>)>,
                 LogLocation, std::variant<std::string, std::unique_ptr<LogRedirect>>) {}

Manager::Manager(std::string /* agentCapability */) {}

Manager::~Manager() {}

Status Manager::getDefaultAdapter(std::shared_ptr<Adapter>& adapter) {
    if (g_managerStub) return g_managerStub->getDefaultAdapter(adapter);
    return Status(StatusCodes::BLUETOOTH_ERROR, "No manager stub set");
}

std::vector<std::shared_ptr<Adapter>> Manager::getAdapters() {
    return {};
}

// ── Uuid static constants ────────────────────────────────────────────────────

const Uuid Uuid::ServiceClasses::AudioSink{uint16_t(0x110B)};
const Uuid Uuid::ServiceClasses::AudioSource{uint16_t(0x110A)};

} // namespace bluetooth
