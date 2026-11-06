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

// L1 tests for the bluetooth-sdk backed IBtAdapter implementation. The real BtSdkAdapterImpl runs against the
// GMock bluetooth::Manager/Adapter stubs, so the SDK backend is exercised in every L1 run.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>

#include "bluetoothSDKMock.h"
#include "BtSdkAdapterImpl.h"

using ::testing::_;
using ::testing::NiceMock;

namespace {

using WPEFramework::Plugin::BtAuthCallbacks;
using WPEFramework::Plugin::BtEventCallbacks;
using WPEFramework::Plugin::BtSdkAdapterImpl;

const Status kSdkFailure(StatusCodes::BLUETOOTH_ERROR, "sdk failure");

class BluetoothSdkAdapterTest : public ::testing::Test {
protected:
    NiceMock<MockManagerStub> manager;
    std::shared_ptr<NiceMock<MockAdapter>> adapter;
    std::unique_ptr<BtSdkAdapterImpl> sdkAdapter;

    void SetUp() override
    {
        adapter = std::make_shared<NiceMock<MockAdapter>>();
        sdkAdapter = std::make_unique<BtSdkAdapterImpl>();
        bluetooth::g_managerStub = &manager;

        ON_CALL(manager, getDefaultAdapter(_))
            .WillByDefault(::testing::Invoke(
                [this](std::shared_ptr<bluetooth::Adapter>& out) {
                    out = adapter;
                    return Status{};
                }));
        ON_CALL(*adapter, getDevices())
            .WillByDefault(::testing::Return(std::vector<std::shared_ptr<bluetooth::Device>>{}));
        ON_CALL(*adapter, getDevices(_))
            .WillByDefault(::testing::Return(std::vector<std::shared_ptr<bluetooth::Device>>{}));
    }

    void TearDown() override
    {
        sdkAdapter->deinit();
        sdkAdapter.reset();
        bluetooth::g_managerStub = nullptr;
        adapter.reset();
    }

    std::string initAdapter()
    {
        return sdkAdapter->init(nullptr, BtEventCallbacks(), BtAuthCallbacks());
    }
};

} // namespace

TEST_F(BluetoothSdkAdapterTest, init_Success_RegistersForAdapterEvents)
{
    EXPECT_CALL(manager, getDefaultAdapter(_)).Times(1);
    EXPECT_CALL(*adapter, registerForEvents(_)).Times(1);

    EXPECT_EQ(std::string(""), initAdapter());
}

TEST_F(BluetoothSdkAdapterTest, init_NoDefaultAdapter_ReportsError)
{
    EXPECT_CALL(manager, getDefaultAdapter(_)).WillOnce(::testing::Return(kSdkFailure));
    EXPECT_CALL(*adapter, registerForEvents(_)).Times(0);

    const std::string error = initAdapter();

    EXPECT_EQ(0u, error.find("No Bluetooth adapter found"));
}

TEST_F(BluetoothSdkAdapterTest, deinit_UnregistersAdapterEvents)
{
    ASSERT_EQ(std::string(""), initAdapter());

    EXPECT_CALL(*adapter, unregisterForEvents()).Times(1);
    sdkAdapter->deinit();

    EXPECT_FALSE(sdkAdapter->startScan(""));
}

TEST_F(BluetoothSdkAdapterTest, getAdapterPowered_Success)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, getPowered(_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(true), ::testing::Return(Status{})));

    bool powered = false;
    EXPECT_TRUE(sdkAdapter->getAdapterPowered(powered));
    EXPECT_TRUE(powered);
}

TEST_F(BluetoothSdkAdapterTest, getAdapterPowered_SdkFailure)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, getPowered(_)).WillOnce(::testing::Return(kSdkFailure));

    bool powered = false;
    EXPECT_FALSE(sdkAdapter->getAdapterPowered(powered));
    EXPECT_FALSE(powered);
}

TEST_F(BluetoothSdkAdapterTest, getAdapterPowered_NotInitialised_Fails)
{
    bool powered = false;
    EXPECT_FALSE(sdkAdapter->getAdapterPowered(powered));
}

TEST_F(BluetoothSdkAdapterTest, setAdapterPowered_Success)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, setPowered(true)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*adapter, setPowered(false)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setAdapterPowered(true));
    EXPECT_TRUE(sdkAdapter->setAdapterPowered(false));
}

TEST_F(BluetoothSdkAdapterTest, setAdapterPowered_SdkFailure)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, setPowered(true)).WillOnce(::testing::Return(kSdkFailure));

    EXPECT_FALSE(sdkAdapter->setAdapterPowered(true));
}

TEST_F(BluetoothSdkAdapterTest, setAdapterPowered_NotInitialised_Fails)
{
    EXPECT_FALSE(sdkAdapter->setAdapterPowered(true));
}

TEST_F(BluetoothSdkAdapterTest, getAdapterName_IsUnsupported)
{
    ASSERT_EQ(std::string(""), initAdapter());

    std::string name = "unchanged";
    EXPECT_FALSE(sdkAdapter->getAdapterName(name));
    EXPECT_EQ(std::string("unchanged"), name);
}

TEST_F(BluetoothSdkAdapterTest, setAdapterName_Success)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, setName("NewSdkAdapter")).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setAdapterName("NewSdkAdapter"));
}

// BTMgr copies at most BTRMGR_NAME_LEN_MAX - 1 (63) bytes of the name.
TEST_F(BluetoothSdkAdapterTest, setAdapterName_LongNameIsTruncatedTo63Bytes)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, setName(std::string(63, 'A'))).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setAdapterName(std::string(70, 'A')));
}

TEST_F(BluetoothSdkAdapterTest, setAdapterName_SdkFailure)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, setName(_)).WillOnce(::testing::Return(kSdkFailure));

    EXPECT_FALSE(sdkAdapter->setAdapterName("NewSdkAdapter"));
}

TEST_F(BluetoothSdkAdapterTest, setAdapterName_NotInitialised_Fails)
{
    EXPECT_FALSE(sdkAdapter->setAdapterName("NewSdkAdapter"));
}

TEST_F(BluetoothSdkAdapterTest, discoverable_IsUnsupported)
{
    ASSERT_EQ(std::string(""), initAdapter());

    bool discoverable = true;
    EXPECT_FALSE(sdkAdapter->isAdapterDiscoverable(discoverable));
    EXPECT_FALSE(sdkAdapter->setAdapterDiscoverable(true, 30));
}

TEST_F(BluetoothSdkAdapterTest, startScan_Success)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, startScan(_)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->startScan("HEADPHONES"));
}

TEST_F(BluetoothSdkAdapterTest, startScan_SdkFailure)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, startScan(_)).WillOnce(::testing::Return(kSdkFailure));

    EXPECT_FALSE(sdkAdapter->startScan("HEADPHONES"));
}

TEST_F(BluetoothSdkAdapterTest, startScan_NotInitialised_Fails)
{
    EXPECT_FALSE(sdkAdapter->startScan("HEADPHONES"));
}

TEST_F(BluetoothSdkAdapterTest, stopScan_Success)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, stopScan()).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->stopScan());
}

TEST_F(BluetoothSdkAdapterTest, stopScan_SdkFailure)
{
    ASSERT_EQ(std::string(""), initAdapter());
    EXPECT_CALL(*adapter, stopScan()).WillOnce(::testing::Return(kSdkFailure));

    EXPECT_FALSE(sdkAdapter->stopScan());
}

TEST_F(BluetoothSdkAdapterTest, stopScan_NotInitialised_Fails)
{
    EXPECT_FALSE(sdkAdapter->stopScan());
}

TEST_F(BluetoothSdkAdapterTest, deviceLists_EmptyWhenSdkReportsNoDevices)
{
    ASSERT_EQ(std::string(""), initAdapter());

    std::vector<WPEFramework::Plugin::IBtAdapter::BtDeviceInfo> paired;
    EXPECT_TRUE(sdkAdapter->queryPairedDevices(paired));
    EXPECT_TRUE(paired.empty());
    EXPECT_TRUE(sdkAdapter->getDiscoveredDevices().empty());
    EXPECT_TRUE(sdkAdapter->getConnectedDevices().empty());
}

TEST_F(BluetoothSdkAdapterTest, deviceLists_NotInitialised_AreEmpty)
{
    std::vector<WPEFramework::Plugin::IBtAdapter::BtDeviceInfo> paired;
    EXPECT_FALSE(sdkAdapter->queryPairedDevices(paired));
    EXPECT_TRUE(sdkAdapter->getDiscoveredDevices().empty());
    EXPECT_TRUE(sdkAdapter->getConnectedDevices().empty());
}

TEST_F(BluetoothSdkAdapterTest, deviceOperations_UnknownHandle_Fail)
{
    ASSERT_EQ(std::string(""), initAdapter());

    EXPECT_FALSE(sdkAdapter->pairDevice("9999"));
    EXPECT_FALSE(sdkAdapter->unpairDevice("9999"));
    EXPECT_FALSE(sdkAdapter->connectDevice("9999", "HEADPHONES"));
    EXPECT_FALSE(sdkAdapter->disconnectDevice("9999", "HEADPHONES"));
    EXPECT_EQ(std::string(""), sdkAdapter->getMacForHandle("9999"));
}

TEST(DeviceRegistryTest, deriveHandle_ValidMac_ReturnsDecimalHandle)
{
    EXPECT_EQ(std::string("187723572702975"), WPEFramework::Plugin::DeviceRegistry::deriveHandle("AA:BB:CC:DD:EE:FF"));
    EXPECT_TRUE(WPEFramework::Plugin::DeviceRegistry::isValidHandle("187723572702975"));
}

TEST(DeviceRegistryTest, deriveHandle_MalformedMac_ReturnsInvalidHandle)
{
    for (const char* mac : {"", "AA:BB:CC", "AA-BB-CC-DD-EE-FF", "GG:BB:CC:DD:EE:FF"}) {
        const std::string handle = WPEFramework::Plugin::DeviceRegistry::deriveHandle(mac);
        EXPECT_EQ(std::string(WPEFramework::Plugin::DeviceRegistry::kInvalidHandle), handle) << mac;
        EXPECT_FALSE(WPEFramework::Plugin::DeviceRegistry::isValidHandle(handle)) << mac;
    }
}

TEST(DeviceRegistryTest, isValidHandle_EmptyAndZero_AreInvalid)
{
    EXPECT_FALSE(WPEFramework::Plugin::DeviceRegistry::isValidHandle(""));
    EXPECT_FALSE(WPEFramework::Plugin::DeviceRegistry::isValidHandle("0"));
    EXPECT_TRUE(WPEFramework::Plugin::DeviceRegistry::isValidHandle("1"));
}

TEST(DeviceRegistryTest, registerDevice_MalformedMac_IsNotRegistered)
{
    WPEFramework::Plugin::DeviceRegistry registry;
    registry.registerDevice("bad");
    EXPECT_EQ(std::string(""), registry.getHandleForMac("bad"));
    EXPECT_EQ(std::string(""), registry.getMacForHandle("0"));
}
