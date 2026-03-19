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
#include "btmgrMock.h"
#include "FactoriesImplementation.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "PowerManagerMock.h"
#include "IarmBusMock.h"

#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "secure_wrappermock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

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
    StoreMock *p_storeMock = nullptr;
    BtmgrImplMock *p_btmgrMock = nullptr;
    IarmBusImplMock   *p_iarmBusImplMock = nullptr ;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    explicit BluetoothTest(bool callInit = true)
        : plugin(Core::ProxyType<Plugin::Bluetooth>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        TEST_LOG("BluetoothTest ctor");

        p_storeMock  = new NiceMock <StoreMock>;

        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t id, const std::string& name) -> void* {
                if (name == "org.rdk.PersistentStore") {
                   return reinterpret_cast<void*>(p_storeMock);
                }
                return nullptr;
        }));

        EXPECT_CALL(PowerManagerMock::Mock(), GetPowerState(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](WPEFramework::Exchange::IPowerManager::PowerState& currentState, WPEFramework::Exchange::IPowerManager::PowerState& previousState) -> uint32_t {
                return Core::ERROR_NONE;
            }));
        
        p_btmgrMock = new NiceMock<BtmgrImplMock>;
        Btmgr::setImpl(p_btmgrMock);

        p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);

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

        if (callInit) {
            EXPECT_EQ(string(""), plugin->Initialize(&service));
        }
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

        IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

        Btmgr::setImpl(nullptr);
        if (p_btmgrMock != nullptr)
        {
            delete p_btmgrMock;
            p_btmgrMock = nullptr;
        }

        if (p_storeMock != nullptr)
        {
            delete p_storeMock;
            p_storeMock = nullptr;
        }
    }

    virtual void SetUp()
    {
    }
};

TEST_F(BluetoothTest, getApiVersionNumber_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"version\":1") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_WithTimeout_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_WithTimeoutAndProfile_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5,\"profile\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_NoAdapters_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(0), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"NO_BLUETOOTH_HARDWARE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_StartDiscoveryFailed_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("startScan"), _T("{}"), response));
}

TEST_F(BluetoothTest, stopScanWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopDeviceDiscovery(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));
}

TEST_F(BluetoothTest, isDiscoverableWrapper_True)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_IsAdapterDiscoverable(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":true") != string::npos);
}

TEST_F(BluetoothTest, isDiscoverableWrapper_False)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_IsAdapterDiscoverable(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":false") != string::npos);
}

TEST_F(BluetoothTest, isDiscoverableWrapper_NoAdapters)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(0), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":false") != string::npos);
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Enable_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterDiscoverable(::testing::_, 1, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":true,\"timeout\":10}"), response));
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Disable_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterDiscoverable(::testing::_, 0, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":false}"), response));
}

TEST_F(BluetoothTest, setDiscoverableWrapper_MissingParameter_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDiscoverable"), _T("{}"), response));
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterDiscoverable(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":true}"), response));
}

TEST_F(BluetoothTest, getDiscoveredDevicesWrapper_Success)
{
    BTRMGR_DiscoveredDevicesList_t discoveredDevices;
    memset(&discoveredDevices, 0, sizeof(discoveredDevices));
    discoveredDevices.m_numOfDevices = 1;
    discoveredDevices.m_deviceProperty[0].m_deviceHandle = 123;
    strcpy(discoveredDevices.m_deviceProperty[0].m_name, "TestDevice");
    discoveredDevices.m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET;
    discoveredDevices.m_deviceProperty[0].m_isConnected = 0;
    discoveredDevices.m_deviceProperty[0].m_isPairedDevice = 0;
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDiscoveredDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(discoveredDevices), ::testing::Return(BTRMGR_RESULT_SUCCESS)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoveredDevices"), _T("{}"), response));

    EXPECT_TRUE(response.find("\"discoveredDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getDiscoveredDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDiscoveredDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoveredDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoveredDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getPairedDevicesWrapper_Success)
{
    BTRMGR_PairedDevicesList_t pairedDevices;
    memset(&pairedDevices, 0, sizeof(pairedDevices));
    pairedDevices.m_numOfDevices = 1;
    pairedDevices.m_deviceProperty[0].m_deviceHandle = 456;
    strcpy(pairedDevices.m_deviceProperty[0].m_name, "PairedDevice");
    pairedDevices.m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_SMARTPHONE;
    pairedDevices.m_deviceProperty[0].m_isConnected = 1;
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetPairedDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(pairedDevices), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getPairedDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetPairedDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getConnectedDevicesWrapper_Success)
{
    BTRMGR_ConnectedDevicesList_t connectedDevices;
    memset(&connectedDevices, 0, sizeof(connectedDevices));
    connectedDevices.m_numOfDevices = 1;
    connectedDevices.m_deviceProperty[0].m_deviceHandle = 789;
    strcpy(connectedDevices.m_deviceProperty[0].m_name, "ConnectedDevice");
    connectedDevices.m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_HEADPHONES;
    connectedDevices.m_deviceProperty[0].m_powerStatus = BTRMGR_DEVICE_POWER_ACTIVE;
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetConnectedDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(connectedDevices), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"connectedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getConnectedDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetConnectedDevices(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"connectedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, connectWrapper_Smartphone_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingIn(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_AudioDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingOut(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"456\",\"deviceType\":\"HEADPHONES\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_HIDDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_ConnectToDevice(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_HID))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"789\",\"deviceType\":\"KEYBOARD\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_LEDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_ConnectToDevice(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_LE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"101\",\"deviceType\":\"LE TILE\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("connect"), _T("{\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingIn(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_Smartphone_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingIn(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_AudioDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"456\",\"deviceType\":\"HEADPHONES\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_HIDDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"789\",\"deviceType\":\"KEYBOARD\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("disconnect"), _T("{}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"456\"}"), response));
}

TEST_F(BluetoothTest, setAudioStreamWrapper_Primary_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAudioStreamingOutType(::testing::_, BTRMGR_STREAM_PRIMARY))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"PRIMARY\"}"), response));
}

TEST_F(BluetoothTest, setAudioStreamWrapper_Auxiliary_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAudioStreamingOutType(::testing::_, BTRMGR_STREAM_AUXILIARY))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"AUXILIARY\"}"), response));
}

TEST_F(BluetoothTest, setAudioStreamWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, setAudioStreamWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAudioStreamingOutType(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"PRIMARY\"}"), response));
}

TEST_F(BluetoothTest, pairWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_PairDevice(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("pair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, pairWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pair"), _T("{}"), response));
}

TEST_F(BluetoothTest, pairWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_PairDevice(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("pair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, unpairWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_UnpairDevice(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unpair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, unpairWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unpair"), _T("{}"), response));
}

TEST_F(BluetoothTest, unpairWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_UnpairDevice(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unpair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, enableWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enable"), _T("{}"), response));
}

TEST_F(BluetoothTest, enableWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("enable"), _T("{}"), response));
}

TEST_F(BluetoothTest, disableWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disable"), _T("{}"), response));
}

TEST_F(BluetoothTest, disableWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("disable"), _T("{}"), response));
}

TEST_F(BluetoothTest, getNameWrapper_Success)
{
    char adapterName[] = "TestAdapter";
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterName(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArrayArgument<1>(adapterName, adapterName + strlen(adapterName) + 1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getName"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"name\":\"TestAdapter\"") != string::npos);
}

TEST_F(BluetoothTest, getNameWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterName(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getName"), _T("{}"), response));
}

TEST_F(BluetoothTest, setNameWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterName(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setName"), _T("{\"name\":\"NewName\"}"), response));
}

TEST_F(BluetoothTest, setNameWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterName(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setName"), _T("{\"name\":\"NewName\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Play_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingIn(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PLAY\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Pause_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_PAUSE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PAUSE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Resume_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_PLAY))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"RESUME\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Stop_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_STOP))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"STOP\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_SkipNext_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_NEXT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"SKIP_NEXT\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_SkipPrevious_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_PREVIOUS))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"SKIP_PREV\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Mute_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_MUTE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"AUDIO_MUTE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Unmute_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_UNMUTE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"AUDIO_UNMUTE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_VolumeUp_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_VOLUMEUP))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"VOLUME_UP\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_VolumeDown_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, BTRMGR_MEDIA_CTRL_VOLUMEDOWN))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"VOLUME_DOWN\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_MediaControl(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PAUSE\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_PairingAccepted_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPairingRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_ConnectionRejected_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onConnectionRequest\",\"responseValue\":\"REJECTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_PlaybackAccepted_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPlaybackRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPairingRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, getDeviceInfoWrapper_Success)
{
    BTRMGR_DevicesProperty_t deviceProperty;
    memset(&deviceProperty, 0, sizeof(deviceProperty));
    deviceProperty.m_deviceHandle = 123;
    strcpy(deviceProperty.m_name, "TestDevice");
    deviceProperty.m_deviceType = BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET;
    deviceProperty.m_vendorID = 9999;
    strcpy(deviceProperty.m_deviceAddress, "00:11:22:33:44:55");
    deviceProperty.m_signalLevel = -50;
    deviceProperty.m_rssi = BTRMGR_RSSI_GOOD;
    deviceProperty.m_batteryLevel = 80;
    strcpy(deviceProperty.m_modalias, "usb:v1234p5678");
    strcpy(deviceProperty.m_firmwareRevision, "1.0.0");
    deviceProperty.m_serviceInfo.m_numOfService = 1;
    strcpy(deviceProperty.m_serviceInfo.m_profileInfo[0].m_profile, "A2DP");
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceProperties(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<2>(deviceProperty), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"deviceInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getDeviceInfoWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, getDeviceInfoWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceProperties(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"deviceInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getMediaTrackInfoWrapper_Success)
{
    BTRMGR_MediaTrackInfo_t trackInfo;
    memset(&trackInfo, 0, sizeof(trackInfo));
    strcpy(trackInfo.pcAlbum, "TestAlbum");
    strcpy(trackInfo.pcGenre, "TestGenre");
    strcpy(trackInfo.pcTitle, "TestTitle");
    strcpy(trackInfo.pcArtist, "TestArtist");
    trackInfo.ui32Duration = 240000;
    trackInfo.ui32TrackNumber = 5;
    trackInfo.ui32NumberOfTracks = 12;
    
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetMediaTrackInfo(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<2>(trackInfo), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"trackInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getMediaTrackInfoWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAudioInfo"), _T("{}"), response));
}

TEST_F(BluetoothTest, getMediaTrackInfoWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetMediaTrackInfo(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"trackInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getDeviceVolumeMuteInfoWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceVolumeMute(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<3>(128), ::testing::SetArgPointee<4>(0), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"volumeinfo\"") != string::npos);
}

TEST_F(BluetoothTest, getDeviceVolumeMuteInfoWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, getDeviceVolumeMuteInfoWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceVolumeMute(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"volumeinfo\"") != string::npos);
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetDeviceVolumeMute(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT, 150, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":150,\"mute\":0}"), response));
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_WithMute_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetDeviceVolumeMute(::testing::_, ::testing::_, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT, 100, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":100,\"mute\":1}"), response));
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_Failed)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetDeviceVolumeMute(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":150,\"mute\":0}"), response));
}

TEST_F(BluetoothTest, setAutoConnectWrapper_Enable_Success)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
}

TEST_F(BluetoothTest, setAutoConnectWrapper_Disable_Success)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":false}"), response));
}

TEST_F(BluetoothTest, setAutoConnectWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, getAutoConnectWrapper_Enabled_Success)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));

    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_Disabled_Success)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":false}"), response));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));

    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAutoConnect"), _T("{}"), response));
}

TEST_F(BluetoothTest, getAutoConnectWrapper_NotFound_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"999\"}"), response));
}

// ============================================================================
// Power mode changed tests
// ============================================================================

// Test fixture that pre-populates cache with one HID device via the persistent
// store so that onPowerModeChanged can exercise the "skip HID" branch.
// Derives from BluetoothTest to reuse all lifecycle wiring; only adds
// HID-specific mock setup before calling Initialize.
class BluetoothPowerModeTest : public BluetoothTest {
protected:
    BluetoothPowerModeTest() : BluetoothTest(false)
    {
        TEST_LOG("BluetoothPowerModeTest ctor");

        // Pre-populate persistent store with a HID device so that init()
        // loads it into the paired device cache via updateCacheFromStorage().
        const std::string hidDeviceJson =
            "[{\"deviceID\":\"456\",\"deviceType\":\"HUMAN INTERFACE DEVICE\","
            "\"autoconnect\":0,\"lastConnectTimeUtc\":\"\"}]";
        ON_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgReferee<2>(hidDeviceJson),
                ::testing::Return(Core::ERROR_NONE)));

        // Return device handle 456 from BTRMGR so the device is not scrubbed
        // during updateCacheFromDevice().
        BTRMGR_PairedDevicesList_t hidPairedDevices;
        memset(&hidPairedDevices, 0, sizeof(hidPairedDevices));
        hidPairedDevices.m_numOfDevices = 1;
        hidPairedDevices.m_deviceProperty[0].m_deviceHandle = 456;
        ON_CALL(*p_btmgrMock, BTRMGR_GetPairedDevices(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgPointee<1>(hidPairedDevices),
                ::testing::Return(BTRMGR_RESULT_SUCCESS)));

        EXPECT_CALL(PowerManagerMock::Mock(), GetPowerState(::testing::_, ::testing::_))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Invoke(
                [&](WPEFramework::Exchange::IPowerManager::PowerState& currentState,
                    WPEFramework::Exchange::IPowerManager::PowerState& previousState) -> uint32_t {
                    currentState = WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_ON;
                    previousState = WPEFramework::Exchange::IPowerManager::PowerState::POWER_STATE_ON;
                    return Core::ERROR_NONE;
                }));

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }
};

// --- onPowerModeChanged: unchanged state ---

TEST_F(BluetoothTest, onPowerModeChanged_SameState_NoAction)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}

// --- onPowerModeChanged: ON → STANDBY with non-HID devices ---

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, 123))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

TEST_F(BluetoothTest, onPowerModeChanged_UnknownToStandby_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, 123))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandbyLightSleep_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, 123))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP);
}

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_NonHidDevice_AutoConnectEnabled_NoDisconnect)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_EmptyCache_NoDisconnect)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

// --- onPowerModeChanged: ON → STANDBY with HID device (should be skipped) ---

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_OnToStandby_HidDevice_AutoConnectDisabled_NoDisconnect)
{
    // Device 456 is HID (pre-loaded in cache) with AUTO_CONNECT_STATUS_DISABLED.
    // HID devices must be skipped on power off/standby so they can wake the device.
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

// --- onPowerModeChanged: X → ON ---

TEST_F(BluetoothTest, onPowerModeChanged_StandbyToOn_WithNonHidPairedDevices_EnablesBluetooth)
{
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(0, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_StandbyToOn_OnlyHidDevices_NoBluetoothEnable)
{
    // Only HID device (456) is in cache; non-HID device count = 0.
    // setBluetoothEnabled must NOT be called.
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}

// --- onPowerModeChanged: X → DEEP_SLEEP ---

TEST_F(BluetoothTest, onPowerModeChanged_OnToDeepSleep_NonHidDevice_AlwaysDisconnects)
{
    // Non-HID device with AUTO_CONNECT_STATUS_ENABLED must still be disconnected
    // when entering deep sleep (autoConnectStatus is not checked for deep sleep).
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, 123))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP);
}

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_OnToDeepSleep_HidDevice_NoDisconnect)
{
    // HID device must be skipped when entering deep sleep.
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP);
}

// --- onPowerModeChanged: unhandled transition ---

TEST_F(BluetoothTest, onPowerModeChanged_UnhandledTransition_NoAction)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(::testing::_, ::testing::_)).Times(0);
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(::testing::_, ::testing::_)).Times(0);

    // STANDBY → STANDBY_LIGHT_SLEEP: does not match any if/else-if branch.
    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP);
}
