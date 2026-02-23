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
#include "ThunderPortability.h"

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
    PowerManagerMock *p_powerManagerMock = nullptr;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
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
        TEST_LOG("*** DEBUG: BluetoothTest ctor");

        p_storeMock  = new NiceMock <StoreMock>;
        Store::setImpl(p_storeMock);
        
        p_btmgrMock = new NiceMock<BtmgrImplMock>;
        Btmgr::setImpl(p_btmgrMock);

        p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);

        p_powerManagerMock = new NiceMock<PowerManagerMock>;
        PowerManager::setImpl(p_powerManagerMock);

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 1");

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 2");

        PluginHost::IFactories::Assign(&factoriesImplementation);

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 3");

        Core::IWorkerPool::Assign(&(*workerPool));

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 4");

        workerPool->Run();

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 5");

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 6");

        dispatcher->Activate(&service);

        TEST_LOG("*** DEBUG: BluetoothTest ctor: Mark 7");

        EXPECT_EQ(string(""), plugin->Initialize(&service));

        TEST_LOG("*** DEBUG: BluetoothTest ctor: exit");
    }

    virtual ~BluetoothTest() override
    {
        TEST_LOG("BluetoothTest xtor");
        TEST_LOG("*** DEBUG: BluetoothTest xtor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        PluginHost::IFactories::Assign(nullptr);

        Store::setImpl(nullptr);
        if(p_storeMock != nullptr)        {
            delete p_storeMock;
            p_storeMock = nullptr;
        }

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

        PowerManager::setImpl(nullptr);
        if (p_powerManagerMock != nullptr)
        {
            delete p_powerManagerMock;
            p_powerManagerMock = nullptr;
        }

        TEST_LOG("*** DEBUG: BluetoothTest xtor: exit");
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
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    
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
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](const string&, const string&, string& value) {
            value = "{\"123\":{\"autoConnectStatus\":1}}";
            return Core::ERROR_NONE;
        }));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoConnectStatus\":true") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_Disabled_Success)
{
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](const string&, const string&, string& value) {
            value = "{\"123\":{\"autoConnectStatus\":0}}";
            return Core::ERROR_NONE;
        }));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoConnectStatus\":false") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAutoConnect"), _T("{}"), response));
}

TEST_F(BluetoothTest, getAutoConnectWrapper_NotFound_Failure)
{
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"999\"}"), response));
}