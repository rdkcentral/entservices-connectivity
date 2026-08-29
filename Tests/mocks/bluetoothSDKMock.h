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

// GoogleMock classes for bluetooth-sdk types — used in SDK L2 test builds.
//
// Test fixture setup pattern (mirrors btmgr mock usage):
//
//   auto mockMgr     = std::make_unique<MockManagerStub>();
//   auto mockAdapter = std::make_shared<MockAdapter>();
//   bluetooth::g_managerStub = mockMgr.get();
//
//   EXPECT_CALL(*mockMgr, getDefaultAdapter(_))
//       .WillOnce(DoAll(SetArgReferee<0>(mockAdapter), Return(Status{})));
//   EXPECT_CALL(*mockAdapter, registerForEvents(_)).Times(1);
//   EXPECT_CALL(*mockAdapter, getDevices())
//       .WillRepeatedly(Return(std::vector<std::shared_ptr<bluetooth::Device>>{}));
//
//   ActivateService("org.rdk.Bluetooth.1");
//   // ... test JSON-RPC calls ...
//   DeactivateService("org.rdk.Bluetooth.1");
//   bluetooth::g_managerStub = nullptr;

#pragma once

#include <gmock/gmock.h>

#include <bluetooth/Manager.h>  // IManagerStub + enums
#include <bluetooth/Adapter.h>  // abstract Adapter
#include <bluetooth/Device.h>   // abstract Device

class MockManagerStub : public bluetooth::IManagerStub {
public:
    virtual ~MockManagerStub() = default;

    MOCK_METHOD(Status, getDefaultAdapter,
        (std::shared_ptr<bluetooth::Adapter>&), (override));
};

class MockAdapter : public bluetooth::Adapter {
public:
    virtual ~MockAdapter() = default;

    MOCK_METHOD(Status, startScan,
        (bluetooth::ScanFilter), (override));
    MOCK_METHOD(Status, stopScan, (), (override));

    MOCK_METHOD(std::vector<std::shared_ptr<bluetooth::Device>>, getDevices,
        (), (override));
    MOCK_METHOD(std::vector<std::shared_ptr<bluetooth::Device>>, getDevices,
        (bluetooth::DeviceState), (override));

    MOCK_METHOD(Status, getPowered,  (bool&),        (override));
    MOCK_METHOD(Status, setPowered,  (bool),         (override));

    MOCK_METHOD(void, registerForEvents,
        (std::function<void(bluetooth::AdapterEvent, bluetooth::AdapterEventData)>),
        (override));
    MOCK_METHOD(void, unregisterForEvents, (), (override));
};

class MockDevice : public bluetooth::Device {
public:
    virtual ~MockDevice() = default;

    MOCK_METHOD(Status, address,        (std::string&), (override));
    MOCK_METHOD(Status, name,           (std::string&), (override));
    MOCK_METHOD(bluetooth::DeviceState, state, (),      (override));
    MOCK_METHOD(void, state, (bluetooth::DeviceState),  (override));
    MOCK_METHOD(Status, getAllProperties,
        (bluetooth::DeviceProperties&), (override));

    MOCK_METHOD(void, registerForEvents,
        (std::function<void(bluetooth::DeviceEvent, std::shared_ptr<bluetooth::Device>)>),
        (override));
    MOCK_METHOD(void, unregisterForEvents, (), (override));

    MOCK_METHOD(Status, connect,    (bool, uint8_t), (override));
    MOCK_METHOD(Status, pair,       (bool, uint8_t), (override));
    MOCK_METHOD(Status, disconnect, (bool, uint8_t), (override));
    MOCK_METHOD(Status, unpair,     (),              (override));
};
