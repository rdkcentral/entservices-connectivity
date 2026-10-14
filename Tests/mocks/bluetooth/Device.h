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

// Stub for bluetooth-sdk bluetooth/Device.h — used in SDK L2 test builds.
//
// bluetooth::Device is an abstract class here so that MockDevice (in
// bluetoothSDKMock.h) can subclass it.  BtSdkAdapterImpl only ever holds
// devices as shared_ptr<Device> obtained from the mock manager/adapter.
#pragma once

#include <Events.h>
#include <Status.h>
#include <bluetooth/GattClient.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bluetooth {

class Device;
class Adapter;
class Manager;

class DeviceProperties {
public:
    std::optional<std::string>                              address;
    std::optional<std::string>                              addressType;
    std::optional<std::string>                              name;
    std::optional<std::string>                              alias;
    std::optional<uint32_t>                                 classOfDevice;
    std::optional<uint16_t>                                 appearance;
    std::optional<std::string>                              icon;
    std::optional<bool>                                     paired;
    std::optional<bool>                                     bonded;
    std::optional<bool>                                     trusted;
    std::optional<bool>                                     blocked;
    std::optional<bool>                                     legacyPairing;
    std::optional<int16_t>                                  rssi;
    std::optional<bool>                                     connected;
    std::optional<std::vector<std::string>>                 uuids;
    std::optional<std::string>                              modalias;
    std::optional<std::string>                              adapter;
    std::optional<std::map<uint16_t, std::vector<uint8_t>>> manufacturerData;
    std::optional<std::map<std::string, std::vector<uint8_t>>> serviceData;
    std::optional<int16_t>                                  txPower;
    std::optional<bool>                                     servicesResolved;
    std::optional<bool>                                     wakeAllowed;
    std::optional<uint8_t>                                  batteryLevel;
};

enum class DeviceEvent {
    Paired,
    Connected,
    Disconnected,
    ServicesResolved,
    ServicesUnresolved,
    Unpaired
};

enum class DeviceState {
    Invalid,
    Paired,
    Discovered,
    Connected,
};

class Device {
public:
    virtual ~Device() = default;

    virtual Status address(std::string& str) = 0;
    virtual Status name(std::string& str) = 0;
    virtual DeviceState state() = 0;
    virtual void state(DeviceState s) = 0;
    virtual Status getAllProperties(DeviceProperties& properties) = 0;

    // EventEmitter<DeviceEvent, shared_ptr<Device>> interface
    virtual void registerForEvents(
        std::function<void(DeviceEvent, std::shared_ptr<Device>)> cb) = 0;
    virtual void unregisterForEvents() = 0;

    virtual Status connect(bool sync = false, uint8_t timeout = 10) = 0;
    virtual Status pair(bool sync = false, uint8_t timeout = 10) = 0;
    virtual Status disconnect(bool sync = false, uint8_t timeout = 10) = 0;
    virtual Status unpair() = 0;
};

} // namespace bluetooth
