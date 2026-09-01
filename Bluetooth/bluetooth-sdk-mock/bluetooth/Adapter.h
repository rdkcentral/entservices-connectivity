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

// Placeholder for bluetooth-sdk's bluetooth/Adapter.h — see Status.h in the
// parent directory for why this file exists and when to remove it.
//
// bluetooth::Adapter is abstract here (no sdbus-c++ proxy base) for the same
// reason as Device.h: real instances are constructed inside librdk_bluetooth.so
// and handed back as shared_ptr<Adapter>, so only the vtable shape needs to
// match across the .so boundary, not any hidden data layout.
#pragma once

#include <Events.h>
#include <Status.h>
#include <bluetooth/Advertisement.h>
#include <bluetooth/Device.h>
#include <bluetooth/Uuid.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bluetooth {

class Adapter;
class Manager;

enum class AdapterEvent {
    DiscoveryStarted,
    DiscoveryStopped,
    PoweredOn,
    PoweredOff,
    DeviceDiscovered,
    DeviceDisappeared
};

enum class ScanType {
    AllDevices,
    LeOnly,
    ClassicOnly
};

struct ScanFilter {
    ScanFilter() : type(ScanType::AllDevices), pattern("") {}
    ScanType             type;
    std::vector<Uuid>    uuids;
    std::string          pattern;
};

struct AdapterEventData {
    std::shared_ptr<Adapter> adapter;
    std::shared_ptr<Device>  device;
};

class Adapter {
public:
    virtual ~Adapter() = default;

    virtual Status startScan(ScanFilter filter = ScanFilter()) = 0;
    virtual Status stopScan() = 0;

    virtual std::vector<std::shared_ptr<Device>> getDevices() = 0;
    virtual std::vector<std::shared_ptr<Device>> getDevices(DeviceState state) = 0;

    virtual Status getPowered(bool& powered) = 0;
    virtual Status setPowered(bool powered) = 0;

    virtual void registerForEvents(
        std::function<void(AdapterEvent, AdapterEventData)> cb) = 0;
    virtual void unregisterForEvents() = 0;
};

} // namespace bluetooth
