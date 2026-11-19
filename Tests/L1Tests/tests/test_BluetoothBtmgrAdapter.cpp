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

// L1 tests for the BTMgr backed IBtAdapter implementation. The real BtMgrAdapterImpl runs against the
// BtmgrImplMock, so the BTMgr backend is exercised in every L1 run.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>
#include <string>
#include <vector>

#include "btmgrMock.h"
#include "BtMgrAdapterImpl.h"

using ::testing::_;
using ::testing::NiceMock;

namespace {

using WPEFramework::Plugin::BtMgrAdapterImpl;
using WPEFramework::Plugin::IBtAdapter;

constexpr BTRMgrDeviceHandle kHandle = 12345ULL;
constexpr const char* kHandleStr = "12345";
constexpr const char* kMac = "AA:BB:CC:DD:EE:FF";
constexpr const char* kDeviceName = "TestBTDevice";

class BluetoothBtmgrAdapterTest : public ::testing::Test {
protected:
    NiceMock<BtmgrImplMock>* p_btmgrMock = nullptr;
    BtMgrAdapterImpl btmgrAdapter;

    void SetUp() override
    {
        p_btmgrMock = new NiceMock<BtmgrImplMock>;
        Btmgr::setImpl(p_btmgrMock);

        ON_CALL(*p_btmgrMock, BTRMGR_GetDeviceTypeAsString(_))
            .WillByDefault(::testing::Return("HEADPHONES"));
    }

    void TearDown() override
    {
        Btmgr::setImpl(nullptr);
        delete p_btmgrMock;
        p_btmgrMock = nullptr;
    }
};

} // namespace

TEST_F(BluetoothBtmgrAdapterTest, getAdapterPowered_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterPowerStatus(0, _))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));

    bool powered = false;
    EXPECT_TRUE(btmgrAdapter.getAdapterPowered(powered));
    EXPECT_TRUE(powered);
}

TEST_F(BluetoothBtmgrAdapterTest, getAdapterPowered_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterPowerStatus(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    bool powered = true;
    EXPECT_FALSE(btmgrAdapter.getAdapterPowered(powered));
    EXPECT_TRUE(powered);
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterPowered_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(0, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(0, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.setAdapterPowered(true));
    EXPECT_TRUE(btmgrAdapter.setAdapterPowered(false));
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterPowered_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterPowerStatus(0, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.setAdapterPowered(true));
}

TEST_F(BluetoothBtmgrAdapterTest, getAdapterName_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterName(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, char* name) {
            std::strcpy(name, "MyAdapter");
            return BTRMGR_RESULT_SUCCESS;
        }));

    std::string name;
    EXPECT_TRUE(btmgrAdapter.getAdapterName(name));
    EXPECT_EQ(std::string("MyAdapter"), name);
}

TEST_F(BluetoothBtmgrAdapterTest, getAdapterName_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetAdapterName(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    std::string name = "unchanged";
    EXPECT_FALSE(btmgrAdapter.getAdapterName(name));
    EXPECT_EQ(std::string("unchanged"), name);
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterName_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterName(0, ::testing::StrEq("NewBtmgrAdapter")))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.setAdapterName("NewBtmgrAdapter"));
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterName_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterName(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.setAdapterName("NewBtmgrAdapter"));
}

TEST_F(BluetoothBtmgrAdapterTest, isAdapterDiscoverable_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_IsAdapterDiscoverable(0, _))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));

    bool discoverable = false;
    EXPECT_TRUE(btmgrAdapter.isAdapterDiscoverable(discoverable));
    EXPECT_TRUE(discoverable);
}

TEST_F(BluetoothBtmgrAdapterTest, isAdapterDiscoverable_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_IsAdapterDiscoverable(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    bool discoverable = false;
    EXPECT_FALSE(btmgrAdapter.isAdapterDiscoverable(discoverable));
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterDiscoverable_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterDiscoverable(0, 1, 30))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.setAdapterDiscoverable(true, 30));
}

TEST_F(BluetoothBtmgrAdapterTest, setAdapterDiscoverable_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetAdapterDiscoverable(0, 0, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.setAdapterDiscoverable(false, 0));
}

TEST_F(BluetoothBtmgrAdapterTest, startScan_AudioProfile_UsesAudioOutput)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.startScan("HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, startScan_AudioAndHidProfile_UsesCombinedType)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_AND_HID))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.startScan("HEADPHONES, KEYBOARD"));
}

TEST_F(BluetoothBtmgrAdapterTest, startScan_NoAdapters_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(0), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(_, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.startScan("HEADPHONES"));
}

// Matches the legacy plugin: success reflects adapter presence, not the discovery call result.
TEST_F(BluetoothBtmgrAdapterTest, startScan_DiscoveryFailureIsOnlyLogged)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetNumberOfAdapters(_))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(1), ::testing::Return(BTRMGR_RESULT_SUCCESS)));
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartDeviceDiscovery(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_TRUE(btmgrAdapter.startScan("HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, stopScan_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.stopScan());
}

TEST_F(BluetoothBtmgrAdapterTest, stopScan_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopDeviceDiscovery(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.stopScan());
}

TEST_F(BluetoothBtmgrAdapterTest, getDiscoveredDevices_Success_CachesHandleToMac)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDiscoveredDevices(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMGR_DiscoveredDevicesList_t* list) {
            list->m_numOfDevices = 1;
            list->m_deviceProperty[0].m_deviceHandle = kHandle;
            list->m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_HID_GAMEPAD;
            std::strcpy(list->m_deviceProperty[0].m_name, kDeviceName);
            std::strcpy(list->m_deviceProperty[0].m_deviceAddress, kMac);
            list->m_deviceProperty[0].m_isPairedDevice = 1;
            list->m_deviceProperty[0].m_isConnected = 0;
            return BTRMGR_RESULT_SUCCESS;
        }));

    const std::vector<IBtAdapter::BtDeviceInfo> devices = btmgrAdapter.getDiscoveredDevices();

    ASSERT_EQ(1u, devices.size());
    EXPECT_EQ(std::string(kHandleStr), devices[0].handleStr);
    EXPECT_EQ(std::string(kMac), devices[0].mac);
    EXPECT_EQ(std::string(kDeviceName), devices[0].name);
    EXPECT_EQ(std::string("HEADPHONES"), devices[0].deviceType);
    EXPECT_TRUE(devices[0].isGamePad);
    EXPECT_TRUE(devices[0].paired);
    EXPECT_FALSE(devices[0].connected);
    EXPECT_EQ(std::string(kMac), btmgrAdapter.getMacForHandle(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, getDiscoveredDevices_Failure_ReturnsEmpty)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDiscoveredDevices(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_TRUE(btmgrAdapter.getDiscoveredDevices().empty());
}

TEST_F(BluetoothBtmgrAdapterTest, getPairedDevices_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetPairedDevices(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMGR_PairedDevicesList_t* list) {
            list->m_numOfDevices = 1;
            list->m_deviceProperty[0].m_deviceHandle = kHandle;
            list->m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_HID_GAMEPAD;
            std::strcpy(list->m_deviceProperty[0].m_name, kDeviceName);
            std::strcpy(list->m_deviceProperty[0].m_deviceAddress, kMac);
            list->m_deviceProperty[0].m_isConnected = 1;
            return BTRMGR_RESULT_SUCCESS;
        }));

    std::vector<IBtAdapter::BtDeviceInfo> devices;
    EXPECT_TRUE(btmgrAdapter.queryPairedDevices(devices));

    ASSERT_EQ(1u, devices.size());
    EXPECT_EQ(std::string(kHandleStr), devices[0].handleStr);
    EXPECT_TRUE(devices[0].paired);
    EXPECT_TRUE(devices[0].connected);
}

TEST_F(BluetoothBtmgrAdapterTest, getPairedDevices_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetPairedDevices(0, _))
        .WillRepeatedly(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    std::vector<IBtAdapter::BtDeviceInfo> devices;
    EXPECT_FALSE(btmgrAdapter.queryPairedDevices(devices));
    EXPECT_TRUE(devices.empty());
    EXPECT_TRUE(btmgrAdapter.getPairedDevices().empty());
}

TEST_F(BluetoothBtmgrAdapterTest, getConnectedDevices_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetConnectedDevices(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMGR_ConnectedDevicesList_t* list) {
            list->m_numOfDevices = 1;
            list->m_deviceProperty[0].m_deviceHandle = kHandle;
            list->m_deviceProperty[0].m_deviceType = BTRMGR_DEVICE_TYPE_HID_GAMEPAD;
            std::strcpy(list->m_deviceProperty[0].m_name, kDeviceName);
            std::strcpy(list->m_deviceProperty[0].m_deviceAddress, kMac);
            list->m_deviceProperty[0].m_isConnected = 1;
            return BTRMGR_RESULT_SUCCESS;
        }));

    const std::vector<IBtAdapter::BtDeviceInfo> devices = btmgrAdapter.getConnectedDevices();

    ASSERT_EQ(1u, devices.size());
    EXPECT_EQ(std::string(kMac), devices[0].mac);
    EXPECT_TRUE(devices[0].paired);
    EXPECT_TRUE(devices[0].connected);
}

TEST_F(BluetoothBtmgrAdapterTest, getConnectedDevices_Failure_ReturnsEmpty)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetConnectedDevices(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_TRUE(btmgrAdapter.getConnectedDevices().empty());
}

TEST_F(BluetoothBtmgrAdapterTest, pairDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_PairDevice(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.pairDevice(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, pairDevice_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_PairDevice(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.pairDevice(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, pairDevice_InvalidHandle_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_PairDevice(_, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.pairDevice("not-a-handle"));
}

TEST_F(BluetoothBtmgrAdapterTest, unpairDevice_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_UnpairDevice(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.unpairDevice(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, unpairDevice_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_UnpairDevice(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.unpairDevice(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, unpairDevice_InvalidHandle_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_UnpairDevice(_, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.unpairDevice("not-a-handle"));
}

TEST_F(BluetoothBtmgrAdapterTest, connectDevice_AudioDevice_StartsAudioStreamingOut)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingOut(0, kHandle, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.connectDevice(kHandleStr, "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, connectDevice_Smartphone_StartsAudioStreamingIn)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingIn(0, kHandle, BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.connectDevice(kHandleStr, "SMARTPHONE"));
}

TEST_F(BluetoothBtmgrAdapterTest, connectDevice_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingOut(0, kHandle, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.connectDevice(kHandleStr, "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, connectDevice_InvalidHandle_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StartAudioStreamingOut(_, _, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.connectDevice("not-a-handle", "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, disconnectDevice_AudioDevice_StopsAudioStreamingOut)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.disconnectDevice(kHandleStr, "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, disconnectDevice_Smartphone_StopsAudioStreamingIn)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingIn(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.disconnectDevice(kHandleStr, "SMARTPHONE"));
}

TEST_F(BluetoothBtmgrAdapterTest, disconnectDevice_HidDevice_DisconnectsFromDevice)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_DisconnectFromDevice(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    EXPECT_TRUE(btmgrAdapter.disconnectDevice(kHandleStr, "HUMAN INTERFACE DEVICE"));
}

TEST_F(BluetoothBtmgrAdapterTest, disconnectDevice_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_StopAudioStreamingOut(0, kHandle))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.disconnectDevice(kHandleStr, "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, disconnectDevice_InvalidHandle_Fails)
{
    EXPECT_FALSE(btmgrAdapter.disconnectDevice("not-a-handle", "HEADPHONES"));
}

TEST_F(BluetoothBtmgrAdapterTest, getDeviceProperties_Success)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceProperties(0, kHandle, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMgrDeviceHandle, BTRMGR_DevicesProperty_t* prop) {
            std::strcpy(prop->m_name, kDeviceName);
            std::strcpy(prop->m_deviceAddress, kMac);
            prop->m_deviceType = BTRMGR_DEVICE_TYPE_HID_GAMEPAD;
            return BTRMGR_RESULT_SUCCESS;
        }));

    IBtAdapter::BtDeviceProperties props;
    EXPECT_TRUE(btmgrAdapter.getDeviceProperties(kHandleStr, props));
    EXPECT_EQ(std::string(kHandleStr), props.handleStr);
    EXPECT_EQ(std::string(kMac), props.mac);
    EXPECT_EQ(std::string(kDeviceName), props.name);
    EXPECT_EQ(std::string("HEADPHONES"), props.deviceType);
    EXPECT_TRUE(props.isGamePad);
    EXPECT_EQ(std::string(kMac), btmgrAdapter.getMacForHandle(kHandleStr));
}

TEST_F(BluetoothBtmgrAdapterTest, getDeviceProperties_Failure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceProperties(0, kHandle, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    IBtAdapter::BtDeviceProperties props;
    EXPECT_FALSE(btmgrAdapter.getDeviceProperties(kHandleStr, props));
}

TEST_F(BluetoothBtmgrAdapterTest, getDeviceProperties_InvalidHandle_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_GetDeviceProperties(_, _, _)).Times(0);

    IBtAdapter::BtDeviceProperties props;
    EXPECT_FALSE(btmgrAdapter.getDeviceProperties("not-a-handle", props));
}

TEST_F(BluetoothBtmgrAdapterTest, getMacForHandle_UnknownHandle_ReturnsEmpty)
{
    EXPECT_EQ(std::string(""), btmgrAdapter.getMacForHandle("9999"));
}

TEST_F(BluetoothBtmgrAdapterTest, respondToEvent_PairingRequest_Accepted)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMGR_EventResponse_t* rsp) {
            EXPECT_EQ(kHandle, rsp->m_deviceHandle);
            EXPECT_EQ(BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST, rsp->m_eventType);
            EXPECT_EQ(1, rsp->m_eventResp);
            return BTRMGR_RESULT_SUCCESS;
        }));

    EXPECT_TRUE(btmgrAdapter.respondToEvent(kHandleStr, "onPairingRequest", true));
}

TEST_F(BluetoothBtmgrAdapterTest, respondToEvent_ConnectionRequest_Rejected)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(0, _))
        .WillOnce(::testing::Invoke([](unsigned char, BTRMGR_EventResponse_t* rsp) {
            EXPECT_EQ(BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST, rsp->m_eventType);
            EXPECT_EQ(0, rsp->m_eventResp);
            return BTRMGR_RESULT_SUCCESS;
        }));

    EXPECT_TRUE(btmgrAdapter.respondToEvent(kHandleStr, "onConnectionRequest", false));
}

TEST_F(BluetoothBtmgrAdapterTest, respondToEvent_UnrecognisedEvent_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(_, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.respondToEvent(kHandleStr, "onUnknownEvent", true));
}

TEST_F(BluetoothBtmgrAdapterTest, respondToEvent_InvalidHandle_Fails)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(_, _)).Times(0);

    EXPECT_FALSE(btmgrAdapter.respondToEvent("not-a-handle", "onPairingRequest", true));
}

TEST_F(BluetoothBtmgrAdapterTest, respondToEvent_BtmgrFailure)
{
    EXPECT_CALL(*p_btmgrMock, BTRMGR_SetEventResponse(0, _))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    EXPECT_FALSE(btmgrAdapter.respondToEvent(kHandleStr, "onPairingRequest", true));
}
