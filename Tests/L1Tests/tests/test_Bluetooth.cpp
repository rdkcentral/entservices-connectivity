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

#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include "Bluetooth.h"
#include "StoreMock.h"

#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "secure_wrappermock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

#define MOCK_USB_DEVICE_BUS_NUMBER_1    100
#define MOCK_USB_DEVICE_ADDRESS_1       001
#define MOCK_USB_DEVICE_PORT_1          123

#define MOCK_USB_DEVICE_BUS_NUMBER_2    101
#define MOCK_USB_DEVICE_ADDRESS_2       002
#define MOCK_USB_DEVICE_PORT_2          124

#define MOCK_USB_DEVICE_SERIAL_NO "0401805e4532973503374df52a239c898397d348"
#define MOCK_USB_DEVICE_MANUFACTURER "USB"
#define MOCK_USB_DEVICE_PRODUCT "SanDisk 3.2Gen1"
#define LIBUSB_CONFIG_ATT_BUS_POWERED 0x80
namespace {
const string callSign = _T("Bluetooth");
}

class BluetoothTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::Bluetooth> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    StoreMock  *p_storeMock = nullptr;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceAttached = nullptr;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceDetached = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    BluetoothTest()
        : plugin(Core::ProxyType<Plugin::Bluetooth>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        TEST_LOG("BluetoothTest ctor");

        p_storeMock  = new NiceMock <StoreMock>;

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }

    virtual ~BluetoothTest() override
    {
        TEST_LOG("BluetoothTest xtor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        PluginHost::IFactories::Assign(nullptr);

        libusbApi::setImpl(nullptr);
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(libUSBHotPlugCbDeviceAttached != nullptr);
    }
};

TEST_F(BluetoothTest, GetApiVersionNumber_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("version")) != string::npos);
}

TEST_F(BluetoothTest, Enable_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enable"), _T("{}"), response));
}

TEST_F(BluetoothTest, Disable_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disable"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetName_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getName"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("name")) != string::npos);
}

TEST_F(BluetoothTest, SetName_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setName"), _T("{\"name\":\"TestDevice\"}"), response));
}

TEST_F(BluetoothTest, SetName_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setName"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetDiscoverable_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("discoverable")) != string::npos);
}

TEST_F(BluetoothTest, SetDiscoverable_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":true,\"timeout\":30}"), response));
}

TEST_F(BluetoothTest, SetDiscoverable_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDiscoverable"), _T("{}"), response));
}

TEST_F(BluetoothTest, StartScan_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":10,\"profile\":\"DEFAULT\"}"), response));
}

TEST_F(BluetoothTest, StartScan_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("startScan"), _T("{}"), response));
}

TEST_F(BluetoothTest, StopScan_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetDiscoveredDevices_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoveredDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("discoveredDevices")) != string::npos);
}

TEST_F(BluetoothTest, Pair_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("pair"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\"}"), response));
}

TEST_F(BluetoothTest, Pair_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pair"), _T("{}"), response));
}

TEST_F(BluetoothTest, Unpair_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unpair"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\"}"), response));
}

TEST_F(BluetoothTest, Unpair_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unpair"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetPairedDevices_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("pairedDevices")) != string::npos);
}

TEST_F(BluetoothTest, Connect_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"deviceType\":\"SMARTPHONE\",\"profile\":\"HUMAN_INTERFACE_DEVICE\"}"), response));
}

TEST_F(BluetoothTest, Connect_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("connect"), _T("{}"), response));
}

TEST_F(BluetoothTest, Disconnect_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, Disconnect_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("disconnect"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetConnectedDevices_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find(_T("connectedDevices")) != string::npos);
}

TEST_F(BluetoothTest, GetDeviceInfo_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\"}"), response));
    EXPECT_TRUE(response.find(_T("deviceInfo")) != string::npos);
}

TEST_F(BluetoothTest, GetDeviceInfo_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetDeviceVolumeMuteInfo_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\"}"), response));
    EXPECT_TRUE(response.find(_T("muted")) != string::npos);
}

TEST_F(BluetoothTest, GetDeviceVolumeMuteInfo_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, SetDeviceVolumeMuteInfo_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"muted\":true}"), response));
}

TEST_F(BluetoothTest, SetDeviceVolumeMuteInfo_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetAudioInfo_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioInfo"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\"}"), response));
    EXPECT_TRUE(response.find(_T("audioInfo")) != string::npos);
}

TEST_F(BluetoothTest, GetAudioInfo_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAudioInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, SendAudioPlaybackCommand_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"command\":\"PLAY\"}"), response));
}

TEST_F(BluetoothTest, SendAudioPlaybackCommand_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{}"), response));
}

TEST_F(BluetoothTest, SetEventResponse_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setEventResponse"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"eventType\":\"onPairingRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, SetEventResponse_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setEventResponse"), _T("{}"), response));
}

TEST_F(BluetoothTest, GetEventResponse_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getEventResponse"), _T("{\"deviceID\":\"AA:BB:CC:DD:EE:FF\",\"eventType\":\"onPairingRequest\"}"), response));
    EXPECT_TRUE(response.find(_T("responseValue")) != string::npos);
}

TEST_F(BluetoothTest, GetEventResponse_InvalidParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getEventResponse"), _T("{}"), response));
}

