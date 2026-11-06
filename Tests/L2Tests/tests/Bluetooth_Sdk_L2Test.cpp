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
#include "BtAdapter.h"
#include "BtSdkAdapterImpl.h"

#include <chrono>
#include <climits>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

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

// BtAdapter::ensureImpl() has no way to find a real bluetooth-sdk library in
// a test environment, so this test injects the real bluetooth::Manager/Adapter
// -based adapter directly, the same way the L1 tests inject their mock.
Plugin::BtSdkAdapterImpl g_sdkAdapterImpl;
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
    Plugin::BtAdapter::setImpl(&g_sdkAdapterImpl);
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
    Plugin::BtAdapter::setImpl(nullptr);
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

TEST_F(Bluetooth_Sdk_L2Test, BluetoothGetNameUnsupported)
{
    JsonObject result;

    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "getName", result));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothSetName_Success)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*g_adapter, setName("NewSdkAdapter"))
        .WillOnce(::testing::Return(Status{}));
    params["name"] = "NewSdkAdapter";
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "setName", params, result));
    EXPECT_TRUE(result["success"].Boolean());
}

// BTMgr copies at most BTRMGR_NAME_LEN_MAX - 1 (63) bytes of the name.
TEST_F(Bluetooth_Sdk_L2Test, BluetoothSetName_LongNameIsTruncatedTo63Bytes)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*g_adapter, setName(std::string(63, 'A')))
        .WillOnce(::testing::Return(Status{}));
    params["name"] = std::string(70, 'A');
    EXPECT_EQ(Core::ERROR_NONE,
              InvokeServiceMethod("org.rdk.Bluetooth.1", "setName", params, result));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(Bluetooth_Sdk_L2Test, BluetoothSetName_SdkFailureIsReported)
{
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*g_adapter, setName("NewSdkAdapter"))
        .WillOnce(::testing::Return(Status(StatusCodes::BLUETOOTH_ERROR, "setName failed")));
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

    EXPECT_CALL(*g_adapter, pruneDiscoveredDevices(::testing::IsNull(), 0u)).Times(1);
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

namespace {

using namespace std::chrono_literals;

// Drives the startup reconnect of a standalone BtSdkAdapterImpl, so that each test decides what the plugin
// side reports (last connect time, legacy autoconnect mark, autoconnect status) and how the client answers.
// All identifiers it records are device handles.
class StartupReconnectHarness {
public:
    enum class Answer { Accept, Reject, None, AutoAccept };

    struct Spec {
        Spec(const std::string& macAddress, long long lastConnect, Answer clientAnswer)
            : mac(macAddress), lastConnectTimeUtc(lastConnect), answer(clientAnswer) {}
        Spec& legacyMarked() { marked = true; return *this; }
        Spec& connectFails() { connectSucceeds = false; return *this; }
        Spec& alreadyConnected() { connectedProperty = true; return *this; }
        Spec& listedAsConnected() { listedConnected = true; return *this; }

        std::string mac;
        long long lastConnectTimeUtc;
        Answer answer;
        bool marked = false;
        bool connectSucceeds = true;
        bool connectedProperty = false;
        bool listedConnected = false;
    };

    static std::string handleOf(const std::string& mac) { return Plugin::DeviceRegistry::deriveHandle(mac); }

    ~StartupReconnectHarness()
    {
        m_adapter.deinit();
        const std::vector<std::shared_ptr<bluetooth::Device>> none;
        ON_CALL(*g_adapter, getDevices()).WillByDefault(::testing::Return(none));
        ON_CALL(*g_adapter, getDevices(::testing::_)).WillByDefault(::testing::Return(none));
        ON_CALL(*g_adapter, getPowered(::testing::_)).WillByDefault(::testing::Return(Status{}));
    }

    void addDevice(const Spec& spec)
    {
        auto device = std::make_shared<::testing::NiceMock<MockDevice>>();
        const std::string mac = spec.mac;
        bluetooth::DeviceProperties props;
        props.uuids = std::vector<std::string>{"0000110b-0000-1000-8000-00805f9b34fb"};
        props.connected = spec.connectedProperty;

        ON_CALL(*device, address(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(mac), ::testing::Return(Status{})));
        ON_CALL(*device, name(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(std::string("Speaker ") + mac),
                                            ::testing::Return(Status{})));
        ON_CALL(*device, getAllProperties(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(props), ::testing::Return(Status{})));
        ON_CALL(*device, connect(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([this, mac, ok = spec.connectSucceeds](bool, uint8_t) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_connects.push_back(handleOf(mac));
                }
                return ok ? Status{} : Status(StatusCodes::BLUETOOTH_ERROR, "connect failed");
            }));

        m_devices.push_back(device);
        if (spec.listedConnected) m_connectedDevices.push_back(device);
        m_specs.emplace(handleOf(mac), spec);
    }

    // Initializes the adapter and starts the startup reconnect. False if the adapter did not initialize.
    bool start(bool systemPowerOn = true)
    {
        ON_CALL(*g_adapter, getPowered(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(true), ::testing::Return(Status{})));
        ON_CALL(*g_adapter, getDevices()).WillByDefault(::testing::Return(m_devices));
        ON_CALL(*g_adapter, getDevices(bluetooth::DeviceState::Paired)).WillByDefault(::testing::Return(m_devices));
        ON_CALL(*g_adapter, getDevices(bluetooth::DeviceState::Connected))
            .WillByDefault(::testing::Return(m_connectedDevices));

        Plugin::BtAuthCallbacks auth;
        auth.onConnectionRequest = [this](const std::string& deviceId, const std::string&, const std::string&,
                                          uint32_t, const std::string&, const std::string&) {
            Answer answer = Answer::None;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_requests.push_back(deviceId);
                auto it = m_specs.find(deviceId);
                if (it != m_specs.end()) answer = it->second.answer;
            }
            if (answer == Answer::Accept || answer == Answer::Reject) {
                m_adapter.respondToEvent(deviceId, "", answer == Answer::Accept);
            }
        };
        auth.getConnectAutoResponse = [this](const std::string& handle, bool& accept) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_specs.find(handle);
            if (it == m_specs.end() || it->second.answer != Answer::AutoAccept) return false;
            accept = true;
            return true;
        };
        auth.getLastConnectTime = [this](const std::string& handle, long long& lastConnectTimeUtc) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_specs.find(handle);
            if (it == m_specs.end() || it->second.lastConnectTimeUtc <= 0) return false;
            lastConnectTimeUtc = it->second.lastConnectTimeUtc;
            return true;
        };
        auth.isLegacyAutoConnect = [this](const std::string& handle) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_specs.find(handle);
            return it != m_specs.end() && it->second.marked;
        };
        auth.onAutoConnected = [this](const std::string& handle) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_autoConnected.push_back(handle);
        };

        if (!m_adapter.init(nullptr, Plugin::BtEventCallbacks{}, std::move(auth)).empty()) return false;
        m_adapter.setSystemPowerOn(systemPowerOn);
        m_adapter.startAutoConnect();
        return true;
    }

    void setSystemPowerOn(bool on) { m_adapter.setSystemPowerOn(on); }
    void startAutoConnect() { m_adapter.startAutoConnect(); }

    std::vector<std::string> requests() const { return snapshot(m_requests); }
    std::vector<std::string> connects() const { return snapshot(m_connects); }
    std::vector<std::string> autoConnected() const { return snapshot(m_autoConnected); }

    bool waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) return true;
            std::this_thread::sleep_for(20ms);
        }
        return condition();
    }

private:
    std::vector<std::string> snapshot(const std::vector<std::string>& list) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return list;
    }

    Plugin::BtSdkAdapterImpl m_adapter;
    mutable std::mutex m_mutex;
    std::map<std::string, Spec> m_specs;
    std::vector<std::shared_ptr<bluetooth::Device>> m_devices;
    std::vector<std::shared_ptr<bluetooth::Device>> m_connectedDevices;
    std::vector<std::string> m_requests;
    std::vector<std::string> m_connects;
    std::vector<std::string> m_autoConnected;
};

using Answer = StartupReconnectHarness::Answer;
using Spec = StartupReconnectHarness::Spec;

const std::string kSpeakerA = "AA:00:00:00:00:01";
const std::string kSpeakerB = "AA:00:00:00:00:02";

std::vector<std::string> handles(std::initializer_list<std::string> macs)
{
    std::vector<std::string> result;
    for (const auto& mac : macs) result.push_back(StartupReconnectHarness::handleOf(mac));
    return result;
}

// Lets a pass that has finished its decisions end, before asserting that nothing more happened.
void settle()
{
    std::this_thread::sleep_for(500ms);
}

} // namespace

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_ClientAccepts_PluginLeavesTheConnectToTheClient)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Accept));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.requests().empty(); }, 10s));
    settle();
    EXPECT_EQ(handles({kSpeakerA}), harness.requests());
    EXPECT_TRUE(harness.connects().empty());
    EXPECT_TRUE(harness.autoConnected().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_ClientRejects_DeviceIsNotConnected)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Reject));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.requests().empty(); }, 10s));
    settle();
    EXPECT_EQ(handles({kSpeakerA}), harness.requests());
    EXPECT_TRUE(harness.connects().empty());
    EXPECT_TRUE(harness.autoConnected().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_ClientRejectsLegacyMarkedDevice_RejectionIsHonoured)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Reject).legacyMarked());
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.requests().empty(); }, 10s));
    settle();
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_TwoDevices_NewestIsAskedFirstAndOnlyOneIsAsked)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Accept));
    harness.addDevice(Spec(kSpeakerB, 2000, Answer::Accept));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.requests().empty(); }, 10s));
    settle();
    EXPECT_EQ(handles({kSpeakerB}), harness.requests());
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_NewestRejected_NextDeviceIsAskedAndLeftToTheClient)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Accept));
    harness.addDevice(Spec(kSpeakerB, 2000, Answer::Reject));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return harness.requests().size() == 2; }, 10s));
    settle();
    EXPECT_EQ(handles({kSpeakerB, kSpeakerA}), harness.requests());
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_NewestFailsToConnect_NextDeviceIsConnected)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::AutoAccept));
    harness.addDevice(Spec(kSpeakerB, 2000, Answer::AutoAccept).connectFails());
    ASSERT_TRUE(harness.start());

    // Three attempts on the newest device, two seconds apart, before moving on.
    ASSERT_TRUE(harness.waitUntil([&] { return harness.connects().size() >= 4; }, 20s));
    EXPECT_EQ(handles({kSpeakerB, kSpeakerB, kSpeakerB, kSpeakerA}), harness.connects());
    EXPECT_EQ(handles({kSpeakerA}), harness.autoConnected());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_AutoAcceptedDevice_IsConnectedWithoutAskingTheClient)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::AutoAccept));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.connects().empty(); }, 10s));
    EXPECT_TRUE(harness.requests().empty());
    EXPECT_EQ(handles({kSpeakerA}), harness.connects());
    EXPECT_EQ(handles({kSpeakerA}), harness.autoConnected());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_AutoAcceptedDeviceThatIsAlreadyConnected_IsNotConnectedAgain)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::AutoAccept).alreadyConnected());
    ASSERT_TRUE(harness.start());

    settle();
    EXPECT_TRUE(harness.requests().empty());
    EXPECT_TRUE(harness.connects().empty());
    EXPECT_TRUE(harness.autoConnected().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_AudioOutputAlreadyConnected_PassDoesNothing)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Accept).listedAsConnected());
    harness.addDevice(Spec(kSpeakerB, 2000, Answer::Accept));
    ASSERT_TRUE(harness.start());

    settle();
    EXPECT_TRUE(harness.requests().empty());
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_DeviceNeverConnectedBefore_IsNotACandidate)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 0, Answer::Accept).legacyMarked());
    ASSERT_TRUE(harness.start());

    settle();
    EXPECT_TRUE(harness.requests().empty());
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_BoxNotInPowerStateOn_PassDoesNothing)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::Accept).legacyMarked());
    ASSERT_TRUE(harness.start(false));

    settle();
    EXPECT_TRUE(harness.requests().empty());
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_PowerLeavesOnWhileWaitingToAskAgain_NoSecondPassAndNextPowerOnStartsANewOne)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::None).legacyMarked());
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return harness.requests().size() == 1; }, 10s));
    harness.setSystemPowerOn(false);
    settle();
    settle();

    // The pass that was waiting 40 s has ended, so a power on starts a new one at once.
    harness.setSystemPowerOn(true);
    harness.startAutoConnect();
    EXPECT_TRUE(harness.waitUntil([&] { return harness.requests().size() == 2; }, 10s));
    EXPECT_TRUE(harness.connects().empty());
}

TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_PowerLeavesOnBetweenConnectAttempts_NoFurtherAttempts)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::AutoAccept).connectFails());
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return harness.connects().size() == 1; }, 10s));
    harness.setSystemPowerOn(false);

    // Without the power check the two remaining attempts follow two and four seconds after the first.
    std::this_thread::sleep_for(5s);
    EXPECT_EQ(handles({kSpeakerA}), harness.connects());
    EXPECT_TRUE(harness.autoConnected().empty());
}

// Takes about 41 seconds: the pass asks again once after kAutoConnectRetrySeconds (40 s) before it falls back
// on the legacy autoconnect mark.
TEST_F(Bluetooth_Sdk_L2Test, StartupReconnect_ClientNeverAnswers_OnlyTheLegacyMarkedDeviceIsConnectedOnTheSecondPass)
{
    StartupReconnectHarness harness;
    harness.addDevice(Spec(kSpeakerA, 1000, Answer::None).legacyMarked());
    harness.addDevice(Spec(kSpeakerB, 2000, Answer::None));
    ASSERT_TRUE(harness.start());

    ASSERT_TRUE(harness.waitUntil([&] { return !harness.connects().empty(); }, 60s));
    EXPECT_EQ(handles({kSpeakerB, kSpeakerB, kSpeakerA}), harness.requests());
    EXPECT_EQ(handles({kSpeakerA}), harness.connects());
    EXPECT_EQ(handles({kSpeakerA}), harness.autoConnected());
}
namespace {

// Runs a standalone BtSdkAdapterImpl over one mock device, so that getConnectedDevices and getDeviceProperties can be
// checked with the device state and BlueZ properties each test chooses.
class SdkDeviceHarness {
public:
    SdkDeviceHarness(bluetooth::DeviceState state, std::optional<bool> paired, std::optional<int16_t> rssi)
        : m_mac("AA:00:00:00:00:10")
        , m_handle(Plugin::DeviceRegistry::deriveHandle(m_mac))
    {
        auto device = std::make_shared<::testing::NiceMock<MockDevice>>();
        bluetooth::DeviceProperties props;
        props.uuids = std::vector<std::string>{"0000110b-0000-1000-8000-00805f9b34fb"};
        props.paired = paired;
        props.rssi = rssi;
        props.connected = (state == bluetooth::DeviceState::Connected);

        const std::string mac = m_mac;
        ON_CALL(*device, address(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(mac), ::testing::Return(Status{})));
        ON_CALL(*device, name(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(std::string("Speaker ") + mac),
                                            ::testing::Return(Status{})));
        ON_CALL(*device, state()).WillByDefault(::testing::Return(state));
        ON_CALL(*device, getAllProperties(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(props), ::testing::Return(Status{})));

        const std::vector<std::shared_ptr<bluetooth::Device>> all{device};
        const std::vector<std::shared_ptr<bluetooth::Device>> connected =
            (state == bluetooth::DeviceState::Connected) ? all : std::vector<std::shared_ptr<bluetooth::Device>>{};
        ON_CALL(*g_adapter, getPowered(::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(true), ::testing::Return(Status{})));
        ON_CALL(*g_adapter, getDevices()).WillByDefault(::testing::Return(all));
        ON_CALL(*g_adapter, getDevices(::testing::_)).WillByDefault(::testing::Return(all));
        ON_CALL(*g_adapter, getDevices(bluetooth::DeviceState::Connected)).WillByDefault(::testing::Return(connected));
    }

    ~SdkDeviceHarness()
    {
        m_adapter.deinit();
        const std::vector<std::shared_ptr<bluetooth::Device>> none;
        ON_CALL(*g_adapter, getDevices()).WillByDefault(::testing::Return(none));
        ON_CALL(*g_adapter, getDevices(::testing::_)).WillByDefault(::testing::Return(none));
        ON_CALL(*g_adapter, getPowered(::testing::_)).WillByDefault(::testing::Return(Status{}));
    }

    bool start()
    {
        return m_adapter.init(nullptr, Plugin::BtEventCallbacks{}, Plugin::BtAuthCallbacks{}).empty();
    }

    std::vector<Plugin::IBtAdapter::BtDeviceInfo> connectedDevices() const { return m_adapter.getConnectedDevices(); }

    bool properties(Plugin::IBtAdapter::BtDeviceProperties& props) const
    {
        return m_adapter.getDeviceProperties(m_handle, props);
    }

    bool unpair(const std::string& handle) { return m_adapter.unpairDevice(handle); }

private:
    std::string m_mac;
    std::string m_handle;
    Plugin::BtSdkAdapterImpl m_adapter;
};

} // namespace

TEST_F(Bluetooth_Sdk_L2Test, GetConnectedDevices_ReportsActiveStateOne)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Connected, true, std::nullopt);
    ASSERT_TRUE(harness.start());

    const auto devices = harness.connectedDevices();
    ASSERT_EQ(1u, devices.size());
    EXPECT_EQ(1u, devices[0].powerStatus);
}

TEST_F(Bluetooth_Sdk_L2Test, GetDeviceInfo_PairedDevice_ReportsZeroRssiAndSignalStrength)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Paired, true, static_cast<int16_t>(-50));
    ASSERT_TRUE(harness.start());

    Plugin::IBtAdapter::BtDeviceProperties props;
    ASSERT_TRUE(harness.properties(props));
    EXPECT_EQ(0, props.rssi);
    EXPECT_EQ(0, props.signalLevel);
}

TEST_F(Bluetooth_Sdk_L2Test, GetDeviceInfo_PairedFlagAbsent_PairedStateReportsZeroRssiAndSignalStrength)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Paired, std::nullopt, static_cast<int16_t>(-50));
    ASSERT_TRUE(harness.start());

    Plugin::IBtAdapter::BtDeviceProperties props;
    ASSERT_TRUE(harness.properties(props));
    EXPECT_EQ(0, props.rssi);
    EXPECT_EQ(0, props.signalLevel);
}

TEST_F(Bluetooth_Sdk_L2Test, GetDeviceInfo_UnpairedDevice_ReportsRawRssiAndMappedSignalStrength)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Discovered, false, static_cast<int16_t>(-55));
    ASSERT_TRUE(harness.start());

    Plugin::IBtAdapter::BtDeviceProperties props;
    ASSERT_TRUE(harness.properties(props));
    EXPECT_EQ(-55, props.rssi);
    EXPECT_EQ(4, props.signalLevel);
}

TEST_F(Bluetooth_Sdk_L2Test, GetDeviceInfo_UnpairedDeviceWithoutRssi_ReportsLegacyUnknownRssiAndPoorSignal)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Discovered, false, std::nullopt);
    ASSERT_TRUE(harness.start());

    Plugin::IBtAdapter::BtDeviceProperties props;
    ASSERT_TRUE(harness.properties(props));
    EXPECT_EQ(INT_MIN, props.rssi);
    EXPECT_EQ(1, props.signalLevel);
}

TEST_F(Bluetooth_Sdk_L2Test, GetDeviceInfo_ConnectedButUnpairedDevice_IsHandledAsUnpaired)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Connected, false, static_cast<int16_t>(-75));
    ASSERT_TRUE(harness.start());

    Plugin::IBtAdapter::BtDeviceProperties props;
    ASSERT_TRUE(harness.properties(props));
    EXPECT_EQ(-75, props.rssi);
    EXPECT_EQ(2, props.signalLevel);
}

// BTMgr fails to unpair a device that is not in its paired list.
TEST_F(Bluetooth_Sdk_L2Test, UnpairDevice_UnknownHandle_Fails)
{
    SdkDeviceHarness harness(bluetooth::DeviceState::Paired, true, std::nullopt);
    ASSERT_TRUE(harness.start());

    EXPECT_FALSE(harness.unpair("999"));
}
