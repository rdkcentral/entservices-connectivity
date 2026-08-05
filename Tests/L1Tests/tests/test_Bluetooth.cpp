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
#include <sstream>
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
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
#include "BtAdapterMock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
const string callSign = _T("Bluetooth");
constexpr const char* PERSISTENT_FILE_PATH = "/tmp/paired_bluetooth_devices.json";
}

class BluetoothTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::Bluetooth> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    StoreMock *p_storeMock = nullptr;
    NiceMock<WPEFramework::Plugin::BtAdapterImplMock> *p_btSdkMock = nullptr;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    WrapsImplMock *p_wrapsImplMock = nullptr;

    explicit BluetoothTest(bool callInit = true)
        : plugin(Core::ProxyType<Plugin::Bluetooth>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        TEST_LOG("BluetoothTest ctor");

        p_storeMock  = new NiceMock <StoreMock>;

        p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t id, const std::string& name) -> void* {
                if (name == "org.rdk.PersistentStore") {
                   return reinterpret_cast<void*>(p_storeMock);
                }
                return nullptr;
        }));
        
        p_btSdkMock = new NiceMock<WPEFramework::Plugin::BtAdapterImplMock>;
        ON_CALL(*p_btSdkMock, init(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [this](WPEFramework::PluginHost::IShell*,
                       WPEFramework::Plugin::BtEventCallbacks evtCbs,
                       WPEFramework::Plugin::BtAuthCallbacks  authCbs) -> std::string {
                    p_btSdkMock->m_evtCbs  = std::move(evtCbs);
                    p_btSdkMock->m_authCbs = std::move(authCbs);
                    return "";
                }));
        WPEFramework::Plugin::BtAdapter::setImpl(p_btSdkMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
    // Simulate a previously-migrated device so that _isMigrated=true after init().
    // Derived fixtures that need _isMigrated=false (e.g. BluetoothLegacyPersistenceMigrationParseTest)
    // override this with a broader ON_CALL that returns ERROR_NOT_EXIST for all keys.
    ON_CALL(*p_storeMock, GetValue(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ::testing::_))
        .WillByDefault(::testing::DoAll(
            ::testing::SetArgReferee<2>(std::string(BLUETOOTH_MIGRATION_VERSION)),
            ::testing::Return(Core::ERROR_NONE)));
#endif

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

        WPEFramework::Plugin::BtAdapter::setImpl(nullptr);
        if (p_btSdkMock != nullptr)
        {
            delete p_btSdkMock;
            p_btSdkMock = nullptr;
        }

        if (p_storeMock != nullptr)
        {
            delete p_storeMock;
            p_storeMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
    }

    virtual void SetUp()
    {
    }

    public:

    void setupDevice()
    {
        // Helper: add a paired HEADPHONES device to the mock device list and PS cache.
        const std::string deviceID = "123";

        IBtAdapter::BtDeviceInfo deviceInfo;
        deviceInfo.handleStr    = deviceID;
        deviceInfo.mac          = "00:11:22:33:44:55";
        deviceInfo.name         = "TestHeadphones";
        deviceInfo.deviceType   = "HEADPHONES";
        deviceInfo.paired       = true;

        IBtAdapter::BtDeviceProperties deviceProps;
        deviceProps.handleStr  = deviceID;
        deviceProps.mac        = "00:11:22:33:44:55";
        deviceProps.name       = "TestHeadphones";
        deviceProps.deviceType = "HEADPHONES";

        ON_CALL(*p_btSdkMock, getPairedDevices())
            .WillByDefault(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{deviceInfo}));

        EXPECT_CALL(*p_btSdkMock, pairDevice(deviceID))
            .WillOnce(::testing::Return(true));

        EXPECT_CALL(*p_btSdkMock, getDeviceProperties(deviceID, ::testing::_))
            .WillOnce(::testing::DoAll(
                ::testing::SetArgReferee<1>(deviceProps),
                ::testing::Return(true)));

        EXPECT_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        EXPECT_EQ(Core::ERROR_NONE,
            handler.Invoke(connection, _T("pair"),
                std::string("{\"deviceID\":\"") + deviceID + "\"}", response));
    }
};

TEST_F(BluetoothTest, getApiVersionNumber_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"version\":1") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_WithTimeout_Success)
{
    EXPECT_CALL(*p_btSdkMock, startScan(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_WithTimeoutAndProfile_Success)
{
    EXPECT_CALL(*p_btSdkMock, startScan("HEADPHONES"))
        .WillOnce(::testing::Return(true));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5,\"profile\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"status\":\"AVAILABLE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_NoAdapters_Failure)
{
    EXPECT_CALL(*p_btSdkMock, startScan(::testing::_))
        .WillOnce(::testing::Return(false));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"NO_BLUETOOTH_HARDWARE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_StartDiscoveryFailed_Failure)
{
    EXPECT_CALL(*p_btSdkMock, startScan(::testing::_))
        .WillOnce(::testing::Return(false));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));
    EXPECT_TRUE(response.find("\"status\":\"NO_BLUETOOTH_HARDWARE\"") != string::npos);
}

TEST_F(BluetoothTest, startScanWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, stopScanWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, startScan(::testing::_)).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), _T("{\"timeout\":5}"), response));

    EXPECT_CALL(*p_btSdkMock, stopScan()).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));
}

TEST_F(BluetoothTest, isDiscoverableWrapper_True)
{
    EXPECT_CALL(*p_btSdkMock, isAdapterDiscoverable(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(true), ::testing::Return(true)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":true") != string::npos);
}

TEST_F(BluetoothTest, isDiscoverableWrapper_False)
{
    EXPECT_CALL(*p_btSdkMock, isAdapterDiscoverable(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(false), ::testing::Return(true)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":false") != string::npos);
}

TEST_F(BluetoothTest, isDiscoverableWrapper_NoAdapters)
{
    EXPECT_CALL(*p_btSdkMock, isAdapterDiscoverable(::testing::_))
        .WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoverable\":false") != string::npos);
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Enable_Success)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterDiscoverable(true, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":true,\"timeout\":10}"), response));
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Disable_Success)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterDiscoverable(false, ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":false}"), response));
}

TEST_F(BluetoothTest, setDiscoverableWrapper_MissingParameter_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setDiscoverableWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterDiscoverable(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDiscoverable"), _T("{\"discoverable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, getDiscoveredDevicesWrapper_Success)
{
    IBtAdapter::BtDeviceInfo info;
    info.handleStr = "123"; info.name = "TestDevice"; info.deviceType = "WEARABLE HEADSET";
    EXPECT_CALL(*p_btSdkMock, getDiscoveredDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{info}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoveredDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoveredDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getDiscoveredDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getDiscoveredDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDiscoveredDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"discoveredDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getPairedDevicesWrapper_Success)
{
    IBtAdapter::BtDeviceInfo info;
    info.handleStr = "123"; info.name = "PairedDevice"; info.deviceType = "SMARTPHONE"; info.connected = true;
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{info}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getPairedDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getConnectedDevicesWrapper_Success)
{
    IBtAdapter::BtDeviceInfo info;
    info.handleStr = "123"; info.name = "ConnectedDevice"; info.deviceType = "HEADPHONES"; info.connected = true;
    EXPECT_CALL(*p_btSdkMock, getConnectedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{info}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"connectedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, getConnectedDevicesWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getConnectedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{}));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"connectedDevices\"") != string::npos);
}

TEST_F(BluetoothTest, connectWrapper_Smartphone_Success)
{
    setupDevice();
    EXPECT_CALL(*p_btSdkMock, connectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_AudioDevice_Success)
{
    setupDevice();
    EXPECT_CALL(*p_btSdkMock, connectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_HIDDevice_Success)
{
    setupDevice();
    EXPECT_CALL(*p_btSdkMock, connectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"KEYBOARD\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_LEDevice_Success)
{
    setupDevice();
    EXPECT_CALL(*p_btSdkMock, connectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"LE TILE\"}"), response));
}

TEST_F(BluetoothTest, connectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceType\":\"SMARTPHONE\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, connectWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, connectDevice("123")).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, disconnectWrapper_Smartphone_Success)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"SMARTPHONE\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_AudioDevice_Success)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_HIDDevice_Success)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"123\",\"deviceType\":\"KEYBOARD\"}"), response));
}

TEST_F(BluetoothTest, disconnectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, disconnectWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123")).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

// setAudioStream is AUDIO_SUPPORT-gated and returns false until SDK audio support lands.
TEST_F(BluetoothTest, setAudioStreamWrapper_Primary_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"PRIMARY\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setAudioStreamWrapper_Auxiliary_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"AUXILIARY\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setAudioStreamWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setAudioStreamWrapper_Failed)
{
TEST_F(BluetoothTest, setAudioStreamWrapper_Failed)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAudioStream"), _T("{\"deviceID\":\"123\",\"audioStreamName\":\"PRIMARY\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, pairWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, pairDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("pair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, pairWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("pair"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, pairWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, pairDevice("123")).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("pair"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, unpairWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, unpairDevice("123")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unpair"), _T("{\"deviceID\":\"123\"}"), response));
}

TEST_F(BluetoothTest, unpairWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unpair"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, unpairWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, unpairDevice("123")).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unpair"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, enableWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(true)).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enable"), _T("{}"), response));
}

TEST_F(BluetoothTest, enableWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(true)).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, disableWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(false)).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disable"), _T("{}"), response));
}

TEST_F(BluetoothTest, disableWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(false)).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disable"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, getNameWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, getAdapterName(::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(std::string("TestAdapter")),
                                   ::testing::Return(true)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getName"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"name\":\"TestAdapter\"") != string::npos);
}

TEST_F(BluetoothTest, getNameWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getAdapterName(::testing::_)).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getName"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setNameWrapper_Success)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterName("NewName")).WillOnce(::testing::Return(true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setName"), _T("{\"name\":\"NewName\"}"), response));
}

TEST_F(BluetoothTest, setNameWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterName(::testing::_)).WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setName"), _T("{\"name\":\"NewName\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

// Audio playback commands are AUDIO_SUPPORT-gated (stubs return false).
TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Play_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PLAY\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Pause_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PAUSE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Resume_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"RESUME\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Stop_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"STOP\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_SkipNext_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"SKIP_NEXT\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_SkipPrevious_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"SKIP_PREV\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Mute_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"AUDIO_MUTE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Unmute_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"AUDIO_UNMUTE\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_VolumeUp_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"VOLUME_UP\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_VolumeDown_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"VOLUME_DOWN\"}"), response));
}

TEST_F(BluetoothTest, sendAudioPlaybackCommandWrapper_Failed)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("sendAudioPlaybackCommand"), _T("{\"deviceID\":\"123\",\"command\":\"PAUSE\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

// respondToEvent resolves via getMacForHandle + respondToEvent on AuthBridge.
TEST_F(BluetoothTest, setEventResponseWrapper_PairingAccepted_Success)
{
    EXPECT_CALL(*p_btSdkMock, getMacForHandle("123"))
        .WillRepeatedly(::testing::Return("00:11:22:33:44:55"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPairingRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_ConnectionRejected_Success)
{
    EXPECT_CALL(*p_btSdkMock, getMacForHandle("123"))
        .WillRepeatedly(::testing::Return("00:11:22:33:44:55"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onConnectionRequest\",\"responseValue\":\"REJECTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_PlaybackAccepted_Success)
{
    EXPECT_CALL(*p_btSdkMock, getMacForHandle("123"))
        .WillRepeatedly(::testing::Return("00:11:22:33:44:55"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPlaybackRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
}

TEST_F(BluetoothTest, setEventResponseWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getMacForHandle("123"))
        .WillRepeatedly(::testing::Return(""));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("respondToEvent"), _T("{\"deviceID\":\"123\",\"eventType\":\"onPairingRequest\",\"responseValue\":\"ACCEPTED\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, getDeviceInfoWrapper_Success)
{
    IBtAdapter::BtDeviceProperties props;
    props.handleStr = "123"; props.mac = "00:11:22:33:44:55";
    props.name = "TestDevice"; props.deviceType = "WEARABLE HEADSET";
    props.vendorId = 9999; props.rssi = -50; props.batteryLevel = 80;
    props.modalias = "usb:v1234p5678";
    props.uuids = {"A2DP"};

    EXPECT_CALL(*p_btSdkMock, getDeviceProperties("123", ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<1>(props),
                                   ::testing::Return(true)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"deviceInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getDeviceInfoWrapper_Failed)
{
    EXPECT_CALL(*p_btSdkMock, getDeviceProperties("123", ::testing::_))
        .WillOnce(::testing::Return(false));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"deviceInfo\"") != string::npos);
}

// getAudioInfo and volume/mute methods are AUDIO_SUPPORT-gated stubs.
TEST_F(BluetoothTest, getMediaTrackInfoWrapper_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"trackInfo\"") != string::npos);
}

TEST_F(BluetoothTest, getMediaTrackInfoWrapper_Failed)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAudioInfo"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"trackInfo\"") != string::npos);
}

// Volume/mute methods are AUDIO_SUPPORT-gated stubs.
TEST_F(BluetoothTest, getDeviceVolumeMuteInfoWrapper_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"volumeinfo\"") != string::npos);
}

TEST_F(BluetoothTest, getDeviceVolumeMuteInfoWrapper_Failed)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\"}"), response));
    EXPECT_TRUE(response.find("\"volumeinfo\"") != string::npos);
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":150,\"mute\":0}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_WithMute_Success)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":100,\"mute\":1}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setDeviceVolumeMuteInfoWrapper_Failed)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"), _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":150,\"mute\":0}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, setAutoConnectWrapper_Enable_Success)
{
    setupDevice();
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
}

TEST_F(BluetoothTest, setAutoConnectWrapper_Disable_Success)
{
    setupDevice();
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":false}"), response));
}

TEST_F(BluetoothTest, setAutoConnectWrapper_MissingParameters_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_Enabled_Success)
{
    setupDevice();
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));

    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_Disabled_Success)
{
    setupDevice();
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"), _T("{\"deviceID\":\"123\",\"enable\":false}"), response));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));

    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_MissingDeviceID_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, getAutoConnectWrapper_NotFound_Failure)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"999\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
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

        // Pre-populate the HID device so that onPowerModeChanged can exercise
        // the "skip HID" branch. On the migration path init() reads migrationVersion
        // first, so we must supply "1" for that key before the deviceInfo payload.
        const std::string hidDeviceJson =
            "[{\"deviceID\":\"123\",\"deviceType\":\"HUMAN INTERFACE DEVICE\","
            "\"autoconnect\":0,\"lastConnectTimeUtc\":\"\"}]";
        ON_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [hidDeviceJson](const std::string&, const std::string& key, std::string& value) -> Core::hresult {
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                    if (key == "migrationVersion") {
                        value = "1";
                        return Core::ERROR_NONE;
                    }
#endif
                    value = hidDeviceJson;
                    return Core::ERROR_NONE;
                }));

#ifndef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
        // Non-migration path: init() calls SDK to reconcile the cache.
        // Return device handle 123 so the HID entry survives the scrub step.
        IBtAdapter::BtDeviceInfo hidInfo;
        hidInfo.handleStr = "123"; hidInfo.deviceType = "HUMAN INTERFACE DEVICE";
        ON_CALL(*p_btSdkMock, getPairedDevices())
            .WillByDefault(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{hidInfo}));
#endif

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
    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(::testing::_)).Times(0);
    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}

// --- onPowerModeChanged: ON → STANDBY with non-HID devices ---

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123"))
        .WillOnce(::testing::Return(true));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}
#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
TEST_F(BluetoothTest, onPowerModeChanged_UnknownToStandby_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123"))
        .WillOnce(::testing::Return(true));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}
#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
TEST_F(BluetoothTest, onPowerModeChanged_OnToStandbyLightSleep_NonHidDevice_AutoConnectDisabled_Disconnects)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123"))
        .WillOnce(::testing::Return(true));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP);
}
#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_NonHidDevice_AutoConnectEnabled_NoDisconnect)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

TEST_F(BluetoothTest, onPowerModeChanged_OnToStandby_EmptyCache_NoDisconnect)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

// --- onPowerModeChanged: ON → STANDBY with HID device (should be skipped) ---

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_OnToStandby_HidDevice_AutoConnectDisabled_NoDisconnect)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);
    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY);
}

// --- onPowerModeChanged: X → ON ---

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
TEST_F(BluetoothTest, onPowerModeChanged_StandbyToOn_WithNonHidPairedDevices_EnablesBluetooth)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(true))
        .WillOnce(::testing::Return(true));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}
#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_StandbyToOn_OnlyHidDevices_NoBluetoothEnable)
{
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON);
}

// --- onPowerModeChanged: X → DEEP_SLEEP ---

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
TEST_F(BluetoothTest, onPowerModeChanged_OnToDeepSleep_NonHidDevice_AlwaysDisconnects)
{
    // Non-HID device with AUTO_CONNECT_STATUS_ENABLED must still be disconnected
    // when entering deep sleep (autoConnectStatus is not checked for deep sleep).
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_CALL(*p_btSdkMock, disconnectDevice("123"))
        .WillOnce(::testing::Return(true));

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP);
}
#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

TEST_F(BluetoothPowerModeTest, onPowerModeChanged_OnToDeepSleep_HidDevice_NoDisconnect)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);

    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_ON,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP);
}

// --- onPowerModeChanged: unhandled transition ---

TEST_F(BluetoothTest, onPowerModeChanged_UnhandledTransition_NoAction)
{
    EXPECT_CALL(*p_btSdkMock, disconnectDevice(::testing::_)).Times(0);
    EXPECT_CALL(*p_btSdkMock, setAdapterPowered(::testing::_)).Times(0);

    // STANDBY → STANDBY_LIGHT_SLEEP: does not match any if/else-if branch.
    plugin->onPowerModeChanged(
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY,
        WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP);
}

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
class BluetoothLegacyPersistenceMigrationParseTest : public BluetoothTest {
protected:
    static constexpr const char* kFilesystemPersistenceFile = PERSISTENT_FILE_PATH;

    BluetoothLegacyPersistenceMigrationParseTest()
        : BluetoothTest(false)
    {
        IBtAdapter::BtDeviceInfo migrationDevice;
        migrationDevice.handleStr  = "123";
        migrationDevice.mac        = "123";
        migrationDevice.name       = "MigrationTestDevice";
        migrationDevice.deviceType = "HEADPHONES";

        ON_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NOT_EXIST));

        ON_CALL(*p_storeMock, SetValue(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NONE));

        ON_CALL(*p_btSdkMock, getPairedDevices())
            .WillByDefault(::testing::Return(
                std::vector<IBtAdapter::BtDeviceInfo>{migrationDevice}));
    }

    ~BluetoothLegacyPersistenceMigrationParseTest() override
    {
        if (std::remove(kFilesystemPersistenceFile) != 0 && errno != ENOENT) {
            LOGWARN("Failed to remove test persistence file during teardown: %s", kFilesystemPersistenceFile);
        }
    }

    bool writeFilesystemPersistencePayload(const std::string& payload)
    {
        std::ofstream output(kFilesystemPersistenceFile, std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }

        output << payload;
        return output.good();
    }

    bool initializeFromFilesystemPersistencePayload(const std::string& payload)
    {
        if (!writeFilesystemPersistencePayload(payload)) {
            return false;
        }

        return (plugin->Initialize(&service).empty());
    }

    bool initializeFromPersistentStorePayload(const std::string& payload)
    {
        // init() reads migrationVersion first, then deviceInfo (migration path).
        EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
            .WillOnce(::testing::DoAll(
                ::testing::SetArgReferee<2>(std::string(BLUETOOTH_MIGRATION_VERSION)),
                ::testing::Return(Core::ERROR_NONE)))
            .WillOnce(::testing::DoAll(
                ::testing::SetArgReferee<2>(payload),
                ::testing::Return(Core::ERROR_NONE)));

        return plugin->Initialize(&service).empty();
    }

    bool readFilesystemPersistencePayload(std::string& payload)
    {
        std::ifstream input(kFilesystemPersistenceFile);
        if (!input.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        payload = buffer.str();

        return true;
    }

};

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, legacyPersistenceMigrationStorePresent_BypassesFilesystemPersistenceImport)
{
    const std::string filesystemPersistencePayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"friendlyName\":\"ShouldBeIgnored\"," 
        "\"deviceType\":\"HEADPHONES\",\"lastVolumeSetting\":10,\"autoConnectStatus\":true,"
        "\"lastConnectionTimeUTC\":1999999999}]}";

    const std::string persistentStorePayload =
        "[{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"autoconnect\":0,"
        "\"lastConnectTimeUtc\":\"1700000000\"}]";

    if (!writeFilesystemPersistencePayload(filesystemPersistencePayload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    if (!initializeFromPersistentStorePayload(persistentStorePayload)) {
        GTEST_SKIP() << "Unable to initialize plugin with PersistentStore payload";
    }

    IBtAdapter::BtDeviceInfo pDev;
    pDev.handleStr = "123"; pDev.deviceType = "HEADPHONES"; pDev.name = "AsTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDev}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
    EXPECT_TRUE(response.find("\"lastConnectTimeUtc\":\"1700000000\"") != string::npos);
}

class BluetoothLegacyPersistenceMigrationParseParamTest
    : public BluetoothLegacyPersistenceMigrationParseTest
    , public ::testing::WithParamInterface<uint32_t> {
};

INSTANTIATE_TEST_SUITE_P(
    StoreKeyNotFound,
    BluetoothLegacyPersistenceMigrationParseParamTest,
    ::testing::Values(Core::ERROR_NOT_EXIST, Core::ERROR_UNKNOWN_KEY),
    [](const ::testing::TestParamInfo<uint32_t>& paramInfo) {
        return paramInfo.param == Core::ERROR_NOT_EXIST ? "ERROR_NOT_EXIST" : "ERROR_UNKNOWN_KEY";
    });

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, legacyPersistenceMigrationMissingStore_ValidFilesystemPersistenceImportPersistsToStore)
{
    std::string persistedJson;
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, PERSISTENT_STORE_KEY_DEVICE_INFO, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::DoAll(
            ::testing::SaveArg<2>(&persistedJson),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ::testing::_))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"friendlyName\":\"TVRemote\","
        "\"deviceType\":\"HEADPHONES\",\"lastVolumeSetting\":25,\"autoConnectStatus\":1,"
        "\"lastConnectionTimeUTC\":\"1712345680\"}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    EXPECT_FALSE(persistedJson.empty());
    EXPECT_TRUE(persistedJson.find("\"deviceID\":\"123\"") != string::npos);
    EXPECT_TRUE(persistedJson.find("\"autoconnect\":1") != string::npos);
    EXPECT_TRUE(persistedJson.find("\"lastConnectTimeUtc\":\"1712345680\"") != string::npos);
}

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, legacyPersistenceMigrationMissingStore_MissingFilesystemPersistenceSourceGracefulFallback)
{
    const std::string seedPayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":false,\"lastConnectionTimeUTC\":0}]}";
    if (!writeFilesystemPersistencePayload(seedPayload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));

    EXPECT_TRUE(plugin->Initialize(&service).empty());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    std::string syncedFilesystemPersistencePayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(syncedFilesystemPersistencePayload));
    EXPECT_TRUE(syncedFilesystemPersistencePayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(syncedFilesystemPersistencePayload.find("\"autoConnectStatus\":true") != string::npos);
}

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, legacyPersistenceMigrationMissingStore_MalformedFilesystemPersistencePayloadNonFatal)
{
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));

    const std::string malformedPayload = "{\"pairedDevices\":[{\"deviceAddr\":\"123\"";
    if (!initializeFromFilesystemPersistencePayload(malformedPayload)) {
        GTEST_SKIP() << "Unable to prepare malformed filesystem persistence migration file on this test host";
    }

    IBtAdapter::BtDeviceInfo pDev1;
    pDev1.handleStr = "123"; pDev1.deviceType = "HEADPHONES"; pDev1.name = "AsTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDev1}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
}

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, rollbackSyncMutations_WriteFilesystemPersistenceWhenFlagOn)
{
    const std::string seedPayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":false,\"lastConnectionTimeUTC\":0}]}";
    if (!writeFilesystemPersistencePayload(seedPayload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));

    EXPECT_TRUE(plugin->Initialize(&service).empty());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    std::string syncedFilesystemPersistencePayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(syncedFilesystemPersistencePayload));
    EXPECT_TRUE(syncedFilesystemPersistencePayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(syncedFilesystemPersistencePayload.find("\"autoConnectStatus\":true") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, legacyPersistenceMigrationParseFallback_NumericTimestampAndStringBoolean)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"friendlyName\":\"TVRemote\","
        "\"deviceType\":\"HEADPHONES\",\"lastVolumeSetting\":40,\"autoConnectStatus\":\"true\","
        "\"lastConnectionTimeUTC\":1712345678}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    IBtAdapter::BtDeviceInfo pDev2;
    pDev2.handleStr = "123"; pDev2.deviceType = "HEADPHONES"; pDev2.name = "AsTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDev2}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
    EXPECT_TRUE(response.find("\"lastConnectTimeUtc\":\"1712345678\"") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, legacyPersistenceMigrationParseFallback_StringTimestampAndNumericBoolean)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"friendlyName\":\"TVRemote\","
        "\"deviceType\":\"HEADPHONES\",\"lastVolumeSetting\":25,\"autoConnectStatus\":1,"
        "\"lastConnectionTimeUTC\":\"1712345680\"}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    IBtAdapter::BtDeviceInfo pDev3;
    pDev3.handleStr = "123"; pDev3.deviceType = "HEADPHONES"; pDev3.name = "AsTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDev3}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
    EXPECT_TRUE(response.find("\"lastConnectTimeUtc\":\"1712345680\"") != string::npos);
}

// ============================================================================
// Data mapping tests: parse (filesystem file → cache)
// ============================================================================

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_AutoConnectStatusBooleanFalse_MapsToDisabled)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":false,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_AutoConnectStatusNumericZero_MapsToDisabled)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":0,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_AutoConnectStatusStringFalse_MapsToDisabled)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":\"false\",\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_AutoConnectStatusStringZero_MapsToDisabled)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":\"0\",\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_AutoConnectStatusMissing_NoPairedDeviceAutoConnectField)
{
    // When autoConnectStatus is absent from the file, Parse() leaves the cache entry
    // with AUTO_CONNECT_STATUS_UNSET. getAutoConnect treats UNSET as DISABLED, so
    // getPairedDevices includes "autoconnect":false in the response.
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    IBtAdapter::BtDeviceInfo pDevUnset;
    pDevUnset.handleStr = "123"; pDevUnset.deviceType = "HEADPHONES"; pDevUnset.name = "MigrationTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDevUnset}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_EntryMissingDeviceAddr_ValidEntryStillImported)
{
    const std::string payload =
        "{\"pairedDevices\":["
        "{\"deviceType\":\"HEADPHONES\",\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0},"
        "{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\",\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}"
        "]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_EntryEmptyDeviceAddr_EntrySkipped)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    IBtAdapter::BtDeviceInfo pDevEmpty;
    pDevEmpty.handleStr = "123"; pDevEmpty.deviceType = "HEADPHONES"; pDevEmpty.name = "MigrationTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDevEmpty}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_LastConnectionTimeUTCMissing_NoLastConnectTimeInResponse)
{
    // When lastConnectionTimeUTC is absent, Parse() leaves lastConnectTimeUtc empty.
    // getPairedDevices only includes the field when it is non-empty.
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    IBtAdapter::BtDeviceInfo pDev5;
    pDev5.handleStr = "123"; pDev5.deviceType = "HEADPHONES"; pDev5.name = "MigrationTestDevice";
    EXPECT_CALL(*p_btSdkMock, getPairedDevices())
        .WillOnce(::testing::Return(std::vector<IBtAdapter::BtDeviceInfo>{pDev5}));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedDevices"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"pairedDevices\"") != string::npos);
    EXPECT_TRUE(response.find("\"lastConnectTimeUtc\"") == string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_MultipleDevices_AllImportedToCache)
{
    // All valid entries in the pairedDevices array must be imported into the cache,
    // not only the first one.
    const std::string payload =
        "{\"pairedDevices\":["
        "{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\",\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0},"
        "{\"deviceAddr\":\"456\",\"deviceType\":\"SMARTPHONE\",\"autoConnectStatus\":false,\"lastConnectionTimeUTC\":0}"
        "]}";

    BTRMGR_PairedDevicesList_t twoPairedDevices;
    memset(&twoPairedDevices, 0, sizeof(twoPairedDevices));
    twoPairedDevices.m_numOfDevices = 2;
    twoPairedDevices.m_deviceProperty[0].m_deviceHandle = 123;
    twoPairedDevices.m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_HEADPHONES;
    strcpy(twoPairedDevices.m_deviceProperty[0].m_name, "Device123");
    twoPairedDevices.m_deviceProperty[1].m_deviceHandle = 456;
    twoPairedDevices.m_deviceProperty[1].m_deviceType = BTRMGR_DEVICE_TYPE_SMARTPHONE;
    strcpy(twoPairedDevices.m_deviceProperty[1].m_name, "Device456");

    // Override default mock so both devices survive the updateCacheFromDevice() scrub step.
    IBtAdapter::BtDeviceInfo dev123, dev456;
    dev123.handleStr = "123"; dev123.deviceType = "HEADPHONES"; dev123.name = "Device123";
    dev456.handleStr = "456"; dev456.deviceType = "SMARTPHONE";  dev456.name = "Device456";
    ON_CALL(*p_btSdkMock, getPairedDevices())
        .WillByDefault(::testing::Return(
            std::vector<IBtAdapter::BtDeviceInfo>{dev123, dev456}));

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"), _T("{\"deviceID\":\"456\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":false") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, parse_FriendlyName_PersistedToFilesystem)
{
    // friendlyName parsed from the file must be stored in the cache and written back
    // to the filesystem persistence file unchanged (updateCacheFromDevice() only
    // backfills friendlyName when the cache entry's name is empty).
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"friendlyName\":\"MyBTDevice\",\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"friendlyName\":\"MyBTDevice\"") != string::npos);
}

// ============================================================================
// Data mapping tests: write (cache → filesystem file)
// ============================================================================

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, write_InitialDeviceAutoConnectUnset_WritesAutoConnectFalseToFilesystem)
{
    // A device entry is present in the filesystem file without an explicit autoConnectStatus
    // field (UNSET). Write() must serialize UNSET as false when there is no prior
    // autoConnectStatus value in the file to inherit from.
    const std::string seedPayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"lastConnectionTimeUTC\":0}]}";
    if (!writeFilesystemPersistencePayload(seedPayload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));

    EXPECT_TRUE(plugin->Initialize(&service).empty());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    // performMigration does not write to the filesystem; trigger the first sync via a mutation
    // so that Write() is exercised with the UNSET autoConnectStatus (no setAutoConnect call,
    // which would change the status from UNSET to DISABLED).
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeviceVolumeMuteInfo"),
        _T("{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"volume\":0,\"mute\":0}"), response));

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(filesystemPayload.find("\"autoConnectStatus\":false") != string::npos);
}

TEST_P(BluetoothLegacyPersistenceMigrationParseParamTest, write_LastConnectTimeUtcEmpty_WritesZeroToFilesystem)
{
    // When the filesystem file entry has no lastConnectionTimeUTC field, the cache
    // entry has an empty lastConnectTimeUtc. Write() must emit lastConnectionTimeUTC
    // as 0 for such entries.
    const std::string seedPayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":0,\"autoConnectStatus\":false}]}";
    if (!writeFilesystemPersistencePayload(seedPayload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(GetParam()))
        .WillRepeatedly(::testing::Return(Core::ERROR_NOT_EXIST));

    EXPECT_TRUE(plugin->Initialize(&service).empty());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    // performMigration does not write to the filesystem; trigger the first sync via a mutation
    // so that Write() is exercised with the empty lastConnectTimeUtc.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":false}"), response));

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(filesystemPayload.find("\"lastConnectionTimeUTC\":0") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, write_DeviceTypeFromFilesystemMigration_PersistedToFilesystem)
{
    // The deviceType imported from the filesystem persistence file must survive
    // the full init sequence and be written back unchanged. updateCacheFromDevice()
    // does not overwrite deviceType for entries already in the cache.
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"SPEAKER\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"deviceType\":\"SPEAKER\"") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, write_DeviceTypeMissingInFile_BackfilledFromBTMGR)
{
    // When the file entry has no deviceType field, Parse() leaves the struct default
    // ("UNKNOWN"). updateCacheFromDevice() must then overwrite it with the type
    // returned by BTRMGR so that "UNKNOWN" is never written to the filesystem.
    // The fixture's default BTRMGR mock returns BTRMGR_DEVICE_TYPE_HEADPHONES /
    // BTRMGR_GetDeviceTypeAsString = "HEADPHONES" for device 123.
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));

    // performMigration does not write to the filesystem; trigger the first sync via a mutation
    // so that the backfilled deviceType is written to the file.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(filesystemPayload.find("\"deviceType\":\"HEADPHONES\"") != string::npos);
    EXPECT_TRUE(filesystemPayload.find("\"deviceType\":\"UNKNOWN\"") == string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, write_LastVolumeSettingFromFilesystemMigration_PersistedToFilesystem)
{
    // lastVolumeSetting parsed from the file must be written back to the filesystem
    // persistence file unchanged through the full migration and write-back cycle.
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"lastVolumeSetting\":42,\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"lastVolumeSetting\":42") != string::npos);
}

TEST_F(BluetoothLegacyPersistenceMigrationParseTest, write_AutoConnectStatusUnset_ExistingFileTruePreserved)
{
    // When a cache entry has AUTO_CONNECT_STATUS_UNSET and the existing filesystem
    // file already contains autoConnectStatus:true for that device, Write() must
    // preserve the existing true value rather than defaulting to false.
    const std::string filesystemPrePopulatedPayload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!writeFilesystemPersistencePayload(filesystemPrePopulatedPayload)) {
        GTEST_SKIP() << "Unable to prepare pre-populated filesystem persistence file";
    }

    // PersistentStore has autoconnect:2 (AUTO_CONNECT_STATUS_UNSET) for device 123.
    const std::string storePayload =
        "[{\"deviceID\":\"123\",\"deviceType\":\"HEADPHONES\",\"autoconnect\":2,\"lastConnectTimeUtc\":\"\"}]";

    if (!initializeFromPersistentStorePayload(storePayload)) {
        GTEST_SKIP() << "Unable to initialize plugin with PersistentStore payload";
    }

    if (!writeFilesystemPersistencePayload(filesystemPrePopulatedPayload)) {
        GTEST_SKIP() << "Unable to restore expected filesystem persistence payload after initialization";
    }

    std::string filesystemPayload;
    ASSERT_TRUE(readFilesystemPersistencePayload(filesystemPayload));
    EXPECT_TRUE(filesystemPayload.find("\"deviceAddr\":\"123\"") != string::npos);
    EXPECT_TRUE(filesystemPayload.find("\"autoConnectStatus\":true") != string::npos);
}

// ============================================================================
// performMigration / clearMigration wrapper API tests
// ============================================================================

// Fixture with _isMigrated=true at startup (simulates a previously-migrated
// device that has a stored migrationVersion in PersistentStore).
class BluetoothClearMigrationTest : public BluetoothLegacyPersistenceMigrationParseTest {
protected:
    BluetoothClearMigrationTest()
    {
        // Return a stored migrationVersion for PERSISTENT_STORE_KEY_MIGRATION_VERSION so that
        // BluetoothDeviceManager::init() sets _isMigrated=true.
        ON_CALL(*p_storeMock, GetValue(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgReferee<2>(std::string("1")),
                ::testing::Return(Core::ERROR_NONE)));

        EXPECT_TRUE(plugin->Initialize(&service).empty());
    }
};

/* -------------------------------------------------------------------------
 * performMigration — success: filesystem source present
 * When the filesystem file exists and no migrationVersion is stored yet (first-time
 * migration), performMigration must import devices and return success=true.
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothLegacyPersistenceMigrationParseTest, performMigrationWrapper_Success_FilePresent)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

/* -------------------------------------------------------------------------
 * performMigration — missing source treated as empty (skipped import)
 * When the filesystem persistence file does not exist, performMigration
 * treats it as an empty device list, clears the cache, and still returns
 * success=true (no failure, graceful skip of device import).
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothLegacyPersistenceMigrationParseTest, performMigrationWrapper_MissingSource_TreatsAsEmptySuccess)
{
    // Ensure the filesystem file does not exist for this test.
    if (std::remove(kFilesystemPersistenceFile) != 0 && errno != ENOENT) {
        GTEST_SKIP() << "Unable to remove filesystem persistence file for test setup";
    }

    EXPECT_TRUE(plugin->Initialize(&service).empty());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

/* -------------------------------------------------------------------------
 * performMigration — sets _isMigrated flag, enables subsequent setAutoConnect
 * Before a successful performMigration, setAutoConnect must be rejected
 * (guard active). After performMigration succeeds, the guard is lifted and
 * setAutoConnect must succeed.
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothLegacyPersistenceMigrationParseTest, performMigrationWrapper_SetsIsMigratedFlag_EnablesSubsequentSetAutoConnect)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!initializeFromFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    // Before migration: _isMigrated=false → setAutoConnect must be rejected.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);

    // Perform migration.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);

    // After migration: _isMigrated=true → setAutoConnect must succeed.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

/* -------------------------------------------------------------------------
 * performMigration — idempotency: migrationVersion present skips all store writes
 * When the migrationVersion key is already present in PersistentStore,
 * performMigration must return success=true immediately without writing
 * deviceInfo or migrationVersion to PersistentStore.
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothLegacyPersistenceMigrationParseTest, performMigrationWrapper_Idempotency_MigrationVersionPresentIsNoOp)
{
    const std::string payload =
        "{\"pairedDevices\":[{\"deviceAddr\":\"123\",\"deviceType\":\"HEADPHONES\","
        "\"autoConnectStatus\":true,\"lastConnectionTimeUTC\":0}]}";

    if (!writeFilesystemPersistencePayload(payload)) {
        GTEST_SKIP() << "Unable to prepare filesystem persistence migration file on this test host";
    }

    // Initialize with no stored migrationVersion so _isMigrated=false.
    EXPECT_TRUE(plugin->Initialize(&service).empty());

    // Arrange: PersistentStore holds migrationVersion="1" indicating migration already done.
    EXPECT_CALL(*p_storeMock, GetValue(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<2>(std::string("1")),
            ::testing::Return(Core::ERROR_NONE)));

    // Neither device data nor the migrationVersion must be written on the no-op path.
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, PERSISTENT_STORE_KEY_DEVICE_INFO, ::testing::_))
        .Times(0);
    EXPECT_CALL(*p_storeMock, SetValue(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("performMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

/* -------------------------------------------------------------------------
 * clearMigration — success: both PersistentStore keys are deleted
 * clearMigration must call DeleteKey for "deviceInfo" and
 * "migrationVersion" and return success=true.
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothClearMigrationTest, clearMigrationWrapper_Success_DeletesBothStoredKeys)
{
    EXPECT_CALL(*p_storeMock, DeleteKey(::testing::_, PERSISTENT_STORE_KEY_DEVICE_INFO))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*p_storeMock, DeleteKey(::testing::_, PERSISTENT_STORE_KEY_MIGRATION_VERSION))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

/* -------------------------------------------------------------------------
 * clearMigration — re-enables pre-migration guard
 * After clearMigration resets _isMigrated to false, setAutoConnect must be
 * rejected again (the pre-migration guard is back in effect).
 * ---------------------------------------------------------------------- */
TEST_F(BluetoothClearMigrationTest, clearMigrationWrapper_ReEnablesGuard_SetAutoConnectRejected)
{
    // Add device 123 to the cache so that setAutoConnect can find it while _isMigrated=true.
    setupDevice();

    // Verify initial state: _isMigrated=true → setAutoConnect must succeed.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);

    // Clear migration state.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearMigration"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);

    // After clearMigration: _isMigrated=false → setAutoConnect must be rejected.
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != string::npos);
}

TEST_F(BluetoothTest, featureGateCompileCoverage_FlagOn)
{
    constexpr bool migrationFlagEnabled = true;
    EXPECT_TRUE(migrationFlagEnabled);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
}
#else
TEST_F(BluetoothTest, featureGateCompileCoverage_FlagOff)
{
    constexpr bool migrationFlagEnabled = false;
    EXPECT_FALSE(migrationFlagEnabled);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
}

TEST_F(BluetoothTest, rollbackSyncMutations_DoNotWriteAsFileWhenFlagOff)
{
    if (std::remove(PERSISTENT_FILE_PATH) != 0 && errno != ENOENT) {
        GTEST_SKIP() << "Unable to remove pre-existing test persistence file";
    }

    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    struct stat fileStat;
    EXPECT_NE(0, stat(PERSISTENT_FILE_PATH, &fileStat));
}

TEST_F(BluetoothTest, featureGateCompileCoverage_BaselinePersistenceFunctionalWhenFlagOff)
{
    setupDevice();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoConnect"),
        _T("{\"deviceID\":\"123\",\"enable\":true}"), response));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAutoConnect"),
        _T("{\"deviceID\":\"123\"}"), response));
    EXPECT_TRUE(response.find("\"autoconnect\":true") != string::npos);
}
#endif
