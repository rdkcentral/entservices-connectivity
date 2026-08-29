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

#include "L2Tests.h"
#include "L2TestsMock.h"
#include "bluetoothSDKMock.h"

#include <chrono>
#include <memory>
#include <thread>

using namespace WPEFramework;

namespace {

class BluetoothSdkSuiteHarness : public L2TestMocks {
public:
    void TestBody() override
    {
    }

    uint32_t Activate(const char* callsign)
    {
        return ActivateService(callsign);
    }

    uint32_t Deactivate(const char* callsign)
    {
        return DeactivateService(callsign);
    }
};

std::unique_ptr<::testing::NiceMock<MockManagerStub>> g_manager;
std::shared_ptr<::testing::NiceMock<MockAdapter>> g_adapter;
bool g_persistentStoreActive = false;
bool g_bluetoothActive = false;

void ConfigureSdkMocks()
{
    g_manager = std::make_unique<::testing::NiceMock<MockManagerStub>>();
    g_adapter = std::make_shared<::testing::NiceMock<MockAdapter>>();
    bluetooth::g_managerStub = g_manager.get();

    ON_CALL(*g_manager, getDefaultAdapter(::testing::_))
        .WillByDefault(::testing::Invoke(
            [](std::shared_ptr<bluetooth::Adapter>& adapter) {
                adapter = g_adapter;
                return Status{};
            }));
    ON_CALL(*g_adapter, getDevices())
        .WillByDefault(::testing::Return(std::vector<std::shared_ptr<bluetooth::Device>>{}));
    ON_CALL(*g_adapter, getDevices(::testing::_))
        .WillByDefault(::testing::Return(std::vector<std::shared_ptr<bluetooth::Device>>{}));
}

} // namespace

class Bluetooth_Sdk_L2Test : public L2TestMocks {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
};

void Bluetooth_Sdk_L2Test::SetUpTestSuite()
{
    ConfigureSdkMocks();
    BluetoothSdkSuiteHarness suiteHarness;

    uint32_t status = suiteHarness.Activate("org.rdk.PersistentStore");
    g_persistentStoreActive = (status == Core::ERROR_NONE);
    ASSERT_EQ(Core::ERROR_NONE, status);

    for (int retry = 0; retry < 5; ++retry) {
        status = suiteHarness.Activate("org.rdk.Bluetooth");
        if (status == Core::ERROR_NONE) {
            g_bluetoothActive = true;
            break;
        }
        if (status != Core::ERROR_TIMEDOUT && status != Core::ERROR_INPROGRESS) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    ASSERT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(g_bluetoothActive);
}

void Bluetooth_Sdk_L2Test::TearDownTestSuite()
{
    BluetoothSdkSuiteHarness suiteHarness;

    if (g_bluetoothActive) {
        EXPECT_EQ(Core::ERROR_NONE, suiteHarness.Deactivate("org.rdk.Bluetooth"));
    }
    if (g_persistentStoreActive) {
        EXPECT_EQ(Core::ERROR_NONE, suiteHarness.Deactivate("org.rdk.PersistentStore"));
    }

    bluetooth::g_managerStub = nullptr;
    g_adapter.reset();
    g_manager.reset();
    g_bluetoothActive = false;
    g_persistentStoreActive = false;
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothGetApiVersionNumber)
{
    JsonObject result;
    const uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getApiVersionNumber", result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("version"));
    EXPECT_GT(result["version"].Number(), 0u);
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothEnableDisable)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*g_adapter, setPowered(true))
        .WillOnce(::testing::Return(Status{}));
    params["enabled"] = "true";
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "enable", params, result));
    EXPECT_TRUE(result["success"].Boolean());

    EXPECT_CALL(*g_adapter, setPowered(false))
        .WillOnce(::testing::Return(Status{}));
    params["enabled"] = "false";
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "disable", params, result));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothGetSetNameUnsupported)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "getName", result));
    EXPECT_FALSE(result["success"].Boolean());

    params["name"] = "NewSdkAdapter";
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "setName", params, result));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothDiscoverableUnsupportedAndScan)
{
    JsonObject params;
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "isDiscoverable", result));
    EXPECT_FALSE(result["discoverable"].Boolean());

    params["discoverable"] = true;
    params["timeout"] = 30;
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "setDiscoverable", params, result));
    EXPECT_FALSE(result["success"].Boolean());

    EXPECT_CALL(*g_adapter, startScan(::testing::_))
        .WillOnce(::testing::Return(Status{}));
    params["profile"] = "HEADPHONES";
    params["timeout"] = 10;
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "startScan", params, result));
    EXPECT_TRUE(result["success"].Boolean());

    EXPECT_CALL(*g_adapter, stopScan())
        .WillOnce(::testing::Return(Status{}));
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "stopScan", result));
    EXPECT_TRUE(result["success"].Boolean());
}