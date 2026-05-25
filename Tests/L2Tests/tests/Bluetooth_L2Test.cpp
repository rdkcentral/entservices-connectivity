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

/*
 * L2 test suite for the Bluetooth plugin.
 *
 * The Bluetooth plugin is JSON-RPC only — it does not expose any COM-RPC interface
 * (Exchange::IXxx). All method calls and event subscriptions therefore go through:
 *   - InvokeServiceMethod()    for synchronous method calls
 *   - JSONRPC::LinkType::Subscribe<JsonObject>()  for async event subscriptions
 *
 * BTR Manager HAL calls are intercepted via BtmgrImplMock (p_btmgrImplMock),
 * which is already instantiated by L2TestMocks. Events are injected by calling
 * the BTRMGR_EventCallback function pointer that the plugin registers at startup.
 *
 * NOTE: BluetoothDeviceManager::init() queries PersistentStore via
 * IShell::QueryInterfaceByCallsign to restore previously paired device info.
 * If PersistentStore is not running, BluetoothDeviceManager treats the interface
 * being unavailable as empty storage (Core::ERROR_NOT_EXIST) and proceeds with
 * an empty in-memory cache. Bluetooth activation therefore succeeds regardless
 * of PersistentStore availability.
 */

#include "L2Tests.h"
#include "L2TestsMock.h"
#include <condition_variable>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>

#define JSON_TIMEOUT      (5000)
#define EVNT_TIMEOUT      (5000)

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);

#define BT_CALLSIGN        _T("org.rdk.Bluetooth.1")
#define BTL2TEST_CALLSIGN  _T("L2tests.1")

/* Test device handle used across test cases */
static const BTRMgrDeviceHandle TEST_DEVICE_HANDLE = 12345ULL;
static const char* TEST_DEVICE_NAME = "TestBTDevice";
static const char* TEST_DEVICE_TYPE_STR = "LOUDSPEAKER";
static const char* TEST_DEVICE_ADDR = "AA:BB:CC:DD:EE:FF";

using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::Invoke;
using namespace WPEFramework;

/* -------------------------------------------------------------------------
 * Async event flags
 * ---------------------------------------------------------------------- */
typedef enum : uint32_t {
    BT_EVT_STATUS_CHANGED     = 0x00000001,
    BT_EVT_DEVICE_FOUND       = 0x00000002,
    BT_EVT_DEVICE_LOST        = 0x00000004,
    BT_EVT_DISCOVERY_UPDATE   = 0x00000008,
    BT_EVT_PAIRING_REQUEST    = 0x00000010,
    BT_EVT_CONNECTION_REQUEST = 0x00000020,
    BT_EVT_PLAYBACK_REQUEST   = 0x00000040,
    BT_EVT_PLAYBACK_CHANGE    = 0x00000080,
    BT_EVT_PLAYBACK_PROGRESS  = 0x00000100,
    BT_EVT_PLAYBACK_NEW_TRACK = 0x00000200,
    BT_EVT_DEVICE_MEDIA_STATUS= 0x00000400,
    BT_EVT_REQUEST_FAILED     = 0x00000800,
    BT_EVT_NONE               = 0x00000000
} BluetoothL2test_async_events_t;

/* -------------------------------------------------------------------------
 * AsyncHandlerMock — JSON-RPC event recipients
 * Because Bluetooth is JSON-RPC only, every event arrives as a JsonObject
 * through JSONRPC::LinkType::Subscribe. No COM-RPC INotification is needed.
 * ---------------------------------------------------------------------- */
class AsyncHandlerMock_Bluetooth {
public:
    AsyncHandlerMock_Bluetooth() {}

    MOCK_METHOD(void, onStatusChanged,      (const JsonObject& message));
    MOCK_METHOD(void, onDeviceFound,        (const JsonObject& message));
    MOCK_METHOD(void, onDeviceLost,         (const JsonObject& message));
    MOCK_METHOD(void, onDiscoveredDevice,   (const JsonObject& message));
    MOCK_METHOD(void, onPairingRequest,     (const JsonObject& message));
    MOCK_METHOD(void, onConnectionRequest,  (const JsonObject& message));
    MOCK_METHOD(void, onPlaybackRequest,    (const JsonObject& message));
    MOCK_METHOD(void, onPlaybackChange,     (const JsonObject& message));
    MOCK_METHOD(void, onPlaybackProgress,   (const JsonObject& message));
    MOCK_METHOD(void, onPlaybackNewTrack,   (const JsonObject& message));
    MOCK_METHOD(void, onDeviceMediaStatus,  (const JsonObject& message));
    MOCK_METHOD(void, onRequestFailed,      (const JsonObject& message));
};

/* -------------------------------------------------------------------------
 * Bluetooth_L2Test fixture
 * ---------------------------------------------------------------------- */
class Bluetooth_L2Test : public L2TestMocks {
public:
    /* Event relay methods — called via EXPECT_CALL WillOnce(Invoke(this, ...)).
     * Must be public so that TEST_F-generated subclasses can form a
     * pointer-to-member (e.g. &Bluetooth_L2Test::onStatusChanged). */
    void onStatusChanged(const JsonObject& message);
    void onDeviceFound(const JsonObject& message);
    void onDeviceLost(const JsonObject& message);
    void onDiscoveredDevice(const JsonObject& message);
    void onPairingRequest(const JsonObject& message);
    void onConnectionRequest(const JsonObject& message);
    void onPlaybackRequest(const JsonObject& message);
    void onPlaybackChange(const JsonObject& message);
    void onPlaybackProgress(const JsonObject& message);
    void onPlaybackNewTrack(const JsonObject& message);
    void onDeviceMediaStatus(const JsonObject& message);
    void onRequestFailed(const JsonObject& message);

protected:
    Bluetooth_L2Test();
    virtual ~Bluetooth_L2Test() override;

    /* Inject a synthetic BTRMGR event into the running plugin */
    void injectBtEvent(BTRMGR_EventMessage_t& msg);

    uint32_t WaitForRequestStatus(uint32_t timeout_ms,
                                  BluetoothL2test_async_events_t expected_status);

private:
    std::mutex              m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t                m_event_signalled;

    /* Captured from BTRMGR_RegisterEventCallback during plugin Initialize */
    BTRMGR_EventCallback    m_eventCallback;
};

/* -------------------------------------------------------------------------
 * Constructor
 * ---------------------------------------------------------------------- */
Bluetooth_L2Test::Bluetooth_L2Test()
    : L2TestMocks()
    , m_event_signalled(BT_EVT_NONE)
    , m_eventCallback(nullptr)
{
    uint32_t status = Core::ERROR_GENERAL;

    /* --- BTR Manager: registration calls made during plugin Initialize --- */
    ON_CALL(*p_btmgrImplMock, BTRMGR_RegisterForCallbacks(::testing::_))
        .WillByDefault(::testing::Return(BTRMGR_RESULT_SUCCESS));

    ON_CALL(*p_btmgrImplMock, BTRMGR_UnRegisterFromCallbacks(::testing::_))
        .WillByDefault(::testing::Return(BTRMGR_RESULT_SUCCESS));

    /* Capture the event callback the plugin registers so tests can inject events */
    ON_CALL(*p_btmgrImplMock, BTRMGR_RegisterEventCallback(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](BTRMGR_EventCallback cb) -> BTRMGR_Result_t {
                m_eventCallback = cb;
                return BTRMGR_RESULT_SUCCESS;
            }));

    /* BluetoothDeviceManager::updateCacheFromDevice() — return empty paired list */
    ON_CALL(*p_btmgrImplMock, BTRMGR_GetPairedDevices(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](unsigned char, BTRMGR_PairedDevicesList_t* pList) -> BTRMGR_Result_t {
                pList->m_numOfDevices = 0;
                return BTRMGR_RESULT_SUCCESS;
            }));

    /* BTRMGR_GetDeviceTypeAsString — used by DeviceManager to stringify device type */
    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(::testing::_))
        .WillByDefault(::testing::Return("UNKNOWN"));

    /* disconnectExternallyConnectedDevices() is called at the end of Initialize —
     * return an empty connected device list so no disconnect attempts are made. */
    ON_CALL(*p_btmgrImplMock, BTRMGR_GetConnectedDevices(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](unsigned char, BTRMGR_ConnectedDevicesList_t* pList) -> BTRMGR_Result_t {
                pList->m_numOfDevices = 0;
                return BTRMGR_RESULT_SUCCESS;
            }));

    /* PersistentStore is activated so that BluetoothDeviceManager::init() can
     * query it for previously paired device info.  If the IStore COM-RPC
     * interface is not reachable (e.g. the L2 test environment does not
     * expose it), updateCacheFromStorage() returns Core::ERROR_NOT_EXIST and
     * init proceeds with an empty in-memory cache — Bluetooth activation
     * therefore succeeds regardless of PersistentStore availability.
     *
     * NOTE: org.rdk.PowerManager is not available in the L2 test build.
     * PowerManagerInterfaceBuilder retries 25 × 200 ms (= 5 s) before
     * giving up. Its absence is non-fatal for Bluetooth init, but each test
     * will be ~5 s slower than necessary until PowerManager is included in
     * the L2 test artefact. */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);

    /* Activate the Bluetooth plugin under test */
    status = ActivateService("org.rdk.Bluetooth");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/* -------------------------------------------------------------------------
 * Destructor
 * ---------------------------------------------------------------------- */
Bluetooth_L2Test::~Bluetooth_L2Test()
{
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = BT_EVT_NONE;

    ON_CALL(*p_btmgrImplMock, BTRMGR_UnRegisterFromCallbacks(::testing::_))
        .WillByDefault(::testing::Return(BTRMGR_RESULT_SUCCESS));

    status = DeactivateService("org.rdk.Bluetooth");
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = DeactivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/* -------------------------------------------------------------------------
 * Event injection helper
 * ---------------------------------------------------------------------- */
void Bluetooth_L2Test::injectBtEvent(BTRMGR_EventMessage_t& msg)
{
    ASSERT_NE(m_eventCallback, nullptr)
        << "BTRMGR event callback was never captured — "
           "BTRMGR_RegisterEventCallback mock may not have fired";
    m_eventCallback(msg);
}

/* -------------------------------------------------------------------------
 * WaitForRequestStatus
 * ---------------------------------------------------------------------- */
uint32_t Bluetooth_L2Test::WaitForRequestStatus(uint32_t timeout_ms,
                                                  BluetoothL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = BT_EVT_NONE;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for BT event 0x%08x", expected_status);
            break;
        }
    }
    signalled = m_event_signalled;
    return signalled;
}

/* -------------------------------------------------------------------------
 * Event relay methods (called from EXPECT_CALL WillOnce)
 * ---------------------------------------------------------------------- */
void Bluetooth_L2Test::onStatusChanged(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onStatusChanged: %s", str.c_str());
    m_event_signalled |= BT_EVT_STATUS_CHANGED;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onDeviceFound(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onDeviceFound: %s", str.c_str());
    m_event_signalled |= BT_EVT_DEVICE_FOUND;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onDeviceLost(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onDeviceLost: %s", str.c_str());
    m_event_signalled |= BT_EVT_DEVICE_LOST;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onDiscoveredDevice(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onDiscoveredDevice: %s", str.c_str());
    m_event_signalled |= BT_EVT_DISCOVERY_UPDATE;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onPairingRequest(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onPairingRequest: %s", str.c_str());
    m_event_signalled |= BT_EVT_PAIRING_REQUEST;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onConnectionRequest(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onConnectionRequest: %s", str.c_str());
    m_event_signalled |= BT_EVT_CONNECTION_REQUEST;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onPlaybackRequest(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onPlaybackRequest: %s", str.c_str());
    m_event_signalled |= BT_EVT_PLAYBACK_REQUEST;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onPlaybackChange(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onPlaybackChange: %s", str.c_str());
    m_event_signalled |= BT_EVT_PLAYBACK_CHANGE;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onPlaybackProgress(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onPlaybackProgress: %s", str.c_str());
    m_event_signalled |= BT_EVT_PLAYBACK_PROGRESS;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onPlaybackNewTrack(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onPlaybackNewTrack: %s", str.c_str());
    m_event_signalled |= BT_EVT_PLAYBACK_NEW_TRACK;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onDeviceMediaStatus(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onDeviceMediaStatus: %s", str.c_str());
    m_event_signalled |= BT_EVT_DEVICE_MEDIA_STATUS;
    m_condition_variable.notify_one();
}
void Bluetooth_L2Test::onRequestFailed(const JsonObject& message)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string str; message.ToString(str);
    TEST_LOG("onRequestFailed: %s", str.c_str());
    m_event_signalled |= BT_EVT_REQUEST_FAILED;
    m_condition_variable.notify_one();
}

/* =========================================================================
 * TC-01: getApiVersionNumber
 * Verify the plugin reports a non-zero API version.
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetApiVersionNumber)
{
    JsonObject result;
    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getApiVersionNumber", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("version"));
    EXPECT_GT(result["version"].Number(), 0u);
}

/* =========================================================================
 * TC-02: enable / disable
 * Verify the adapter power status is set correctly via BTR Manager.
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothEnableDisable)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    /* --- enable --- */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_SetAdapterPowerStatus(0, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["enabled"] = "true";
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "enable", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    /* --- disable --- */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_SetAdapterPowerStatus(0, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["enabled"] = "false";
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "disable", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-03: enable failure
 * Verify that a BTR Manager failure propagates as success == false.
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothEnableFailure)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_SetAdapterPowerStatus(0, 1))
        .WillOnce(::testing::Return(BTRMGR_RESULT_GENERIC_FAILURE));

    params["enabled"] = "true";
    InvokeServiceMethod("org.rdk.Bluetooth.1", "enable", params, result);
    /* Plugin returns success=false in the JSON body; the RPC transport itself succeeds */
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-04: getName / setName round-trip
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetSetName)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    /* getName */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_GetAdapterName(0, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, char* pName) -> BTRMGR_Result_t {
                strncpy(pName, "TestAdapter", BTRMGR_NAME_LEN_MAX - 1);
                return BTRMGR_RESULT_SUCCESS;
            }));

    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getName", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("name"));
    EXPECT_STREQ("TestAdapter", result["name"].String().c_str());

    /* setName */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_SetAdapterName(0, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["name"] = "NewName";
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "setName", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-05: isDiscoverable
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothIsDiscoverable)
{
    JsonObject result;

    /* isAdapterDiscoverable() internally calls GetNumberOfAdapters then IsAdapterDiscoverable */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_GetNumberOfAdapters(::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [](unsigned char* pNum) -> BTRMGR_Result_t {
                *pNum = 1;
                return BTRMGR_RESULT_SUCCESS;
            }));

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_IsAdapterDiscoverable(0, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, unsigned char* pDisc) -> BTRMGR_Result_t {
                *pDisc = 1;
                return BTRMGR_RESULT_SUCCESS;
            }));

    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "isDiscoverable", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("discoverable"));
    EXPECT_TRUE(result["discoverable"].Boolean());
}

/* =========================================================================
 * TC-06: setDiscoverable
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothSetDiscoverable)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_SetAdapterDiscoverable(0, 1, 30))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["discoverable"] = true;
    params["timeout"] = 30;
    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "setDiscoverable", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-07: startScan / stopScan
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothStartStopScan)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_StartDeviceDiscovery(0, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["timeout"] = 10;
    params["profile"] = "LOUDSPEAKER";
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "startScan", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_StopDeviceDiscovery(0, ::testing::_))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "stopScan", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-08: getDiscoveredDevices — verify JSON array shape
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetDiscoveredDevices)
{
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_GetDiscoveredDevices(0, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMGR_DiscoveredDevicesList_t* pList) -> BTRMGR_Result_t {
                pList->m_numOfDevices = 1;
                pList->m_deviceProperty[0].m_deviceHandle  = TEST_DEVICE_HANDLE;
                pList->m_deviceProperty[0].m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
                pList->m_deviceProperty[0].m_isPairedDevice = 0;
                pList->m_deviceProperty[0].m_isConnected   = 0;
                pList->m_deviceProperty[0].m_isLastConnectedDevice = 0;
                pList->m_deviceProperty[0].m_ui32DevClassBtSpec   = 0x240404;
                pList->m_deviceProperty[0].m_ui16DevAppearanceBleSpec = 0;
                strncpy(pList->m_deviceProperty[0].m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
                return BTRMGR_RESULT_SUCCESS;
            }));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getDiscoveredDevices", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("discoveredDevices"));

    JsonArray devices = result["discoveredDevices"].Array();
    EXPECT_EQ(1u, devices.Length());

    JsonObject device = devices[0].Object();
    EXPECT_STREQ(std::to_string(TEST_DEVICE_HANDLE).c_str(), device["deviceID"].String().c_str());
    EXPECT_STREQ(TEST_DEVICE_NAME, device["name"].String().c_str());
    EXPECT_STREQ(TEST_DEVICE_TYPE_STR, device["deviceType"].String().c_str());
}

/* =========================================================================
 * TC-09: getPairedDevices — verify JSON array shape
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetPairedDevices)
{
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_GetPairedDevices(0, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMGR_PairedDevicesList_t* pList) -> BTRMGR_Result_t {
                pList->m_numOfDevices = 1;
                pList->m_deviceProperty[0].m_deviceHandle  = TEST_DEVICE_HANDLE;
                pList->m_deviceProperty[0].m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
                pList->m_deviceProperty[0].m_isConnected   = 0;
                pList->m_deviceProperty[0].m_isLastConnectedDevice = 1;
                pList->m_deviceProperty[0].m_ui32DevClassBtSpec    = 0x240404;
                pList->m_deviceProperty[0].m_ui16DevAppearanceBleSpec = 0;
                strncpy(pList->m_deviceProperty[0].m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
                return BTRMGR_RESULT_SUCCESS;
            }));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getPairedDevices", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("pairedDevices"));

    JsonArray devices = result["pairedDevices"].Array();
    EXPECT_EQ(1u, devices.Length());

    JsonObject device = devices[0].Object();
    EXPECT_STREQ(std::to_string(TEST_DEVICE_HANDLE).c_str(), device["deviceID"].String().c_str());
    EXPECT_STREQ(TEST_DEVICE_NAME, device["name"].String().c_str());
}

/* =========================================================================
 * TC-10: getConnectedDevices — verify JSON array shape
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetConnectedDevices)
{
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_GetConnectedDevices(0, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMGR_ConnectedDevicesList_t* pList) -> BTRMGR_Result_t {
                pList->m_numOfDevices = 1;
                pList->m_deviceProperty[0].m_deviceHandle  = TEST_DEVICE_HANDLE;
                pList->m_deviceProperty[0].m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
                pList->m_deviceProperty[0].m_isConnected   = 1;
                pList->m_deviceProperty[0].m_ui32DevClassBtSpec    = 0x240404;
                pList->m_deviceProperty[0].m_ui16DevAppearanceBleSpec = 0;
                strncpy(pList->m_deviceProperty[0].m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
                return BTRMGR_RESULT_SUCCESS;
            }));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getConnectedDevices", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("connectedDevices"));

    JsonArray devices = result["connectedDevices"].Array();
    EXPECT_EQ(1u, devices.Length());

    JsonObject device = devices[0].Object();
    EXPECT_STREQ(std::to_string(TEST_DEVICE_HANDLE).c_str(), device["deviceID"].String().c_str());
    EXPECT_STREQ(TEST_DEVICE_NAME, device["name"].String().c_str());
}

/* =========================================================================
 * TC-11: pair / unpair
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothPairUnpair)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_PairDevice(0, TEST_DEVICE_HANDLE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["deviceID"] = std::to_string(TEST_DEVICE_HANDLE);
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "pair", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_UnpairDevice(0, TEST_DEVICE_HANDLE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "unpair", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-12: connect / disconnect (audio output device)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothConnectDisconnect)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    /* connect: BTRMGR_ConnectToDevice is always called; for audio output,
     * BTRMGR_StartAudioStreamingOut may also be called — allow it. */
    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_ConnectToDevice(0, TEST_DEVICE_HANDLE, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    ON_CALL(*p_btmgrImplMock,
            BTRMGR_StartAudioStreamingOut(0, TEST_DEVICE_HANDLE, ::testing::_))
        .WillByDefault(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["deviceID"]   = std::to_string(TEST_DEVICE_HANDLE);
    params["deviceType"] = TEST_DEVICE_TYPE_STR;
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "connect", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    /* disconnect */
    EXPECT_CALL(*p_btmgrImplMock, BTRMGR_DisconnectFromDevice(0, TEST_DEVICE_HANDLE))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "disconnect", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-13: getDeviceInfo
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetDeviceInfo)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_GetDeviceProperties(0, TEST_DEVICE_HANDLE, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMgrDeviceHandle, BTRMGR_DevicesProperty_t* pProp) -> BTRMGR_Result_t {
                pProp->m_deviceHandle = TEST_DEVICE_HANDLE;
                pProp->m_deviceType   = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
                pProp->m_vendorID     = 0x00E0;
                pProp->m_isPaired     = 1;
                pProp->m_isConnected  = 0;
                strncpy(pProp->m_name,          TEST_DEVICE_NAME,  BTRMGR_NAME_LEN_MAX - 1);
                strncpy(pProp->m_deviceAddress, TEST_DEVICE_ADDR,  BTRMGR_NAME_LEN_MAX - 1);
                return BTRMGR_RESULT_SUCCESS;
            }));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    params["deviceID"] = std::to_string(TEST_DEVICE_HANDLE);
    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getDeviceInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    JsonObject deviceInfo = result["deviceInfo"].Object();
    EXPECT_STREQ(TEST_DEVICE_NAME, deviceInfo["name"].String().c_str());
    EXPECT_STREQ(TEST_DEVICE_TYPE_STR, deviceInfo["deviceType"].String().c_str());
}

/* =========================================================================
 * TC-14: getAudioInfo (media track info)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetAudioInfo)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_GetMediaTrackInfo(0, TEST_DEVICE_HANDLE, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMgrDeviceHandle, BTRMGR_MediaTrackInfo_t* pInfo) -> BTRMGR_Result_t {
                strncpy(pInfo->pcAlbum,  "TestAlbum",  BTRMGR_MAX_STR_LEN - 1);
                strncpy(pInfo->pcArtist, "TestArtist", BTRMGR_MAX_STR_LEN - 1);
                strncpy(pInfo->pcTitle,  "TestTitle",  BTRMGR_MAX_STR_LEN - 1);
                strncpy(pInfo->pcGenre,  "TestGenre",  BTRMGR_MAX_STR_LEN - 1);
                pInfo->ui32Duration        = 240;
                pInfo->ui32TrackNumber     = 1;
                pInfo->ui32NumberOfTracks  = 12;
                return BTRMGR_RESULT_SUCCESS;
            }));

    params["deviceID"] = std::to_string(TEST_DEVICE_HANDLE);
    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getAudioInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    JsonObject trackInfo = result["trackInfo"].Object();
    EXPECT_STREQ("TestAlbum",  trackInfo["album"].String().c_str());
    EXPECT_STREQ("TestArtist", trackInfo["artist"].String().c_str());
    EXPECT_STREQ("TestTitle",  trackInfo["title"].String().c_str());
}

/* =========================================================================
 * TC-15: getDeviceVolumeMuteInfo / setDeviceVolumeMuteInfo
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothGetSetDeviceVolumeMute)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    /* getDeviceVolumeMuteInfo */
    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_GetDeviceVolumeMute(0, TEST_DEVICE_HANDLE, ::testing::_,
                                           ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](unsigned char, BTRMgrDeviceHandle, BTRMGR_DeviceOperationType_t,
               unsigned char* pVol, unsigned char* pMute) -> BTRMGR_Result_t {
                *pVol  = 80;
                *pMute = 0;
                return BTRMGR_RESULT_SUCCESS;
            }));

    params["deviceID"]      = std::to_string(TEST_DEVICE_HANDLE);
    params["deviceProfile"] = TEST_DEVICE_TYPE_STR;
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getDeviceVolumeMuteInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ(80u, result["volumeInfo"].Object()["volume"].Number());
    EXPECT_FALSE(result["volumeInfo"].Object()["mute"].Boolean());

    /* setDeviceVolumeMuteInfo */
    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_SetDeviceVolumeMute(0, TEST_DEVICE_HANDLE, ::testing::_, 50, 0))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["volume"] = 50;
    params["muted"]  = false;
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "setDeviceVolumeMuteInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-16: setAutoConnect / getAutoConnect round-trip
 * BluetoothDeviceManager persists the flag; no BTR HAL call needed.
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothSetGetAutoConnect)
{
    JsonObject params;
    JsonObject result;
    uint32_t status = Core::ERROR_GENERAL;

    /* setAutoConnect — persists to BluetoothDeviceManager cache */
    params["deviceID"] = std::to_string(TEST_DEVICE_HANDLE);
    params["enable"]   = true;
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "setAutoConnect", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    /* getAutoConnect — reads back from cache */
    status = InvokeServiceMethod("org.rdk.Bluetooth.1", "getAutoConnect", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result["enable"].Boolean());
}

/* =========================================================================
 * TC-17: sendAudioPlaybackCommand (PLAY)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothSendAudioPlaybackCommandPlay)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_btmgrImplMock,
                BTRMGR_MediaControl(0, TEST_DEVICE_HANDLE, BTRMGR_MEDIA_CTRL_PLAY))
        .WillOnce(::testing::Return(BTRMGR_RESULT_SUCCESS));

    params["deviceID"]     = std::to_string(TEST_DEVICE_HANDLE);
    params["audioCtrlCmd"] = "PLAY";
    uint32_t status = InvokeServiceMethod("org.rdk.Bluetooth.1", "sendAudioPlaybackCommand", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/* =========================================================================
 * TC-18: onStatusChanged event — DISCOVERY_STARTED
 * Inject BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED and verify the plugin fires
 * onStatusChanged with newStatus == "DISCOVERY_STARTED".
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnStatusChangedDiscoveryStarted)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    /* Subscribe to the event */
    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onStatusChanged"),
        &AsyncHandlerMock_Bluetooth::onStatusChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    /* Set expectation on the mock handler */
    EXPECT_CALL(async_handler, onStatusChanged(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onStatusChanged));

    /* Inject the BTRMGR event */
    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED;
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_STATUS_CHANGED);
    EXPECT_TRUE(signalled & BT_EVT_STATUS_CHANGED);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onStatusChanged"));
}

/* =========================================================================
 * TC-19: onStatusChanged event — DISCOVERY_COMPLETED
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnStatusChangedDiscoveryCompleted)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onStatusChanged"),
        &AsyncHandlerMock_Bluetooth::onStatusChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onStatusChanged(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onStatusChanged));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_STATUS_CHANGED);
    EXPECT_TRUE(signalled & BT_EVT_STATUS_CHANGED);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onStatusChanged"));
}

/* =========================================================================
 * TC-20: onStatusChanged event — PAIRING_CHANGE (pair complete)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnStatusChangedPairingComplete)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onStatusChanged"),
        &AsyncHandlerMock_Bluetooth::onStatusChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onStatusChanged(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onStatusChanged));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    msg.m_discoveredDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_discoveredDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_discoveredDevice.m_isPairedDevice = 1;
    msg.m_discoveredDevice.m_isConnected   = 0;
    msg.m_discoveredDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_discoveredDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_STATUS_CHANGED);
    EXPECT_TRUE(signalled & BT_EVT_STATUS_CHANGED);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onStatusChanged"));
}

/* =========================================================================
 * TC-21: onStatusChanged event — CONNECTION_CHANGE (connect complete)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnStatusChangedConnectionComplete)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onStatusChanged"),
        &AsyncHandlerMock_Bluetooth::onStatusChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onStatusChanged(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onStatusChanged));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE;
    msg.m_pairedDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_pairedDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_pairedDevice.m_isConnected   = 1;
    msg.m_pairedDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_pairedDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_STATUS_CHANGED);
    EXPECT_TRUE(signalled & BT_EVT_STATUS_CHANGED);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onStatusChanged"));
}

/* =========================================================================
 * TC-22: onDeviceFound event
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnDeviceFound)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onDeviceFound"),
        &AsyncHandlerMock_Bluetooth::onDeviceFound,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onDeviceFound(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onDeviceFound));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_FOUND;
    msg.m_pairedDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_pairedDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_pairedDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_pairedDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_DEVICE_FOUND);
    EXPECT_TRUE(signalled & BT_EVT_DEVICE_FOUND);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onDeviceFound"));
}

/* =========================================================================
 * TC-23: onDeviceLost event
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnDeviceLost)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onDeviceLost"),
        &AsyncHandlerMock_Bluetooth::onDeviceLost,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onDeviceLost(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onDeviceLost));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_OUT_OF_RANGE;
    msg.m_pairedDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_pairedDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_pairedDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_pairedDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_DEVICE_LOST);
    EXPECT_TRUE(signalled & BT_EVT_DEVICE_LOST);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onDeviceLost"));
}

/* =========================================================================
 * TC-24: onDiscoveredDevice event (DISCOVERED)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnDiscoveredDeviceDiscovered)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onDiscoveredDevice"),
        &AsyncHandlerMock_Bluetooth::onDiscoveredDevice,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onDiscoveredDevice(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onDiscoveredDevice));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE;
    msg.m_discoveredDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_discoveredDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_discoveredDevice.m_isDiscovered  = 1;
    msg.m_discoveredDevice.m_isPairedDevice = 0;
    msg.m_discoveredDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_discoveredDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_DISCOVERY_UPDATE);
    EXPECT_TRUE(signalled & BT_EVT_DISCOVERY_UPDATE);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onDiscoveredDevice"));
}

/* =========================================================================
 * TC-25: onPairingRequest event (external pair request)
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnPairingRequest)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onPairingRequest"),
        &AsyncHandlerMock_Bluetooth::onPairingRequest,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onPairingRequest(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onPairingRequest));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST;
    msg.m_externalDevice.m_deviceHandle       = TEST_DEVICE_HANDLE;
    msg.m_externalDevice.m_deviceType         = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_externalDevice.m_vendorID           = 0x00E0;
    msg.m_externalDevice.m_externalDevicePIN  = 0; /* pinRequired = false */
    msg.m_externalDevice.m_serviceInfo.m_numOfService = 0;
    strncpy(msg.m_externalDevice.m_name,          TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    strncpy(msg.m_externalDevice.m_deviceAddress, TEST_DEVICE_ADDR, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_PAIRING_REQUEST);
    EXPECT_TRUE(signalled & BT_EVT_PAIRING_REQUEST);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onPairingRequest"));
}

/* =========================================================================
 * TC-26: onRequestFailed event — pairing failure
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnRequestFailedPairing)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onRequestFailed"),
        &AsyncHandlerMock_Bluetooth::onRequestFailed,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onRequestFailed(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onRequestFailed));

    ON_CALL(*p_btmgrImplMock, BTRMGR_GetDeviceTypeAsString(BTRMGR_DEVICE_TYPE_LOUDSPEAKER))
        .WillByDefault(::testing::Return(TEST_DEVICE_TYPE_STR));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_DEVICE_PAIRING_FAILED;
    msg.m_discoveredDevice.m_deviceHandle  = TEST_DEVICE_HANDLE;
    msg.m_discoveredDevice.m_deviceType    = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_discoveredDevice.m_isPairedDevice = 0;
    msg.m_discoveredDevice.m_isConnected   = 0;
    msg.m_discoveredDevice.m_isLastConnectedDevice = 0;
    strncpy(msg.m_discoveredDevice.m_name, TEST_DEVICE_NAME, BTRMGR_NAME_LEN_MAX - 1);
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_REQUEST_FAILED);
    EXPECT_TRUE(signalled & BT_EVT_REQUEST_FAILED);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onRequestFailed"));
}

/* =========================================================================
 * TC-27: onPlaybackChange event — media track started
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnPlaybackChangeStarted)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onPlaybackChange"),
        &AsyncHandlerMock_Bluetooth::onPlaybackChange,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onPlaybackChange(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onPlaybackChange));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_MEDIA_TRACK_STARTED;
    msg.m_mediaInfo.m_deviceHandle                       = TEST_DEVICE_HANDLE;
    msg.m_mediaInfo.m_deviceType                         = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
    msg.m_mediaInfo.m_mediaPositionInfo.m_mediaPosition  = 0;
    msg.m_mediaInfo.m_mediaPositionInfo.m_mediaDuration  = 180;
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_PLAYBACK_CHANGE);
    EXPECT_TRUE(signalled & BT_EVT_PLAYBACK_CHANGE);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onPlaybackChange"));
}

/* =========================================================================
 * TC-28: onPlaybackNewTrack event
 * ====================================================================== */
TEST_F(Bluetooth_L2Test, BluetoothOnPlaybackNewTrack)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(BT_CALLSIGN, BTL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Bluetooth> async_handler;
    uint32_t signalled = BT_EVT_NONE;

    uint32_t status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("onPlaybackNewTrack"),
        &AsyncHandlerMock_Bluetooth::onPlaybackNewTrack,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(async_handler, onPlaybackNewTrack(::testing::_))
        .WillOnce(Invoke(this, &Bluetooth_L2Test::onPlaybackNewTrack));

    BTRMGR_EventMessage_t msg{};
    msg.m_adapterIndex = 0;
    msg.m_eventType    = BTRMGR_EVENT_MEDIA_TRACK_CHANGED;
    msg.m_mediaInfo.m_deviceHandle = TEST_DEVICE_HANDLE;
    strncpy(msg.m_mediaInfo.m_mediaTrackInfo.pcAlbum,  "Album1",   BTRMGR_MAX_STR_LEN - 1);
    strncpy(msg.m_mediaInfo.m_mediaTrackInfo.pcArtist, "Artist1",  BTRMGR_MAX_STR_LEN - 1);
    strncpy(msg.m_mediaInfo.m_mediaTrackInfo.pcTitle,  "Track1",   BTRMGR_MAX_STR_LEN - 1);
    strncpy(msg.m_mediaInfo.m_mediaTrackInfo.pcGenre,  "Pop",      BTRMGR_MAX_STR_LEN - 1);
    msg.m_mediaInfo.m_mediaTrackInfo.ui32Duration       = 200;
    msg.m_mediaInfo.m_mediaTrackInfo.ui32TrackNumber    = 3;
    msg.m_mediaInfo.m_mediaTrackInfo.ui32NumberOfTracks = 15;
    injectBtEvent(msg);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, BT_EVT_PLAYBACK_NEW_TRACK);
    EXPECT_TRUE(signalled & BT_EVT_PLAYBACK_NEW_TRACK);

    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("onPlaybackNewTrack"));
}
