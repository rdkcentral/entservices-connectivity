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

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Telemetry.h"
#include "bluetoothSDKMock.h"
#include "BtSdkAdapterImpl.h"
#include "VolumeUtils.h"

using ::testing::_;
using ::testing::NiceMock;

namespace {

using WPEFramework::Plugin::BtAuthCallbacks;
using WPEFramework::Plugin::BtEventCallbacks;
using WPEFramework::Plugin::BtSdkAdapterImpl;

const Status kSdkFailure(StatusCodes::BLUETOOTH_ERROR, "sdk failure");

class SdkTelemetryMock : public TelemetryApiImpl {
public:
    MOCK_METHOD(void, t2_init, (char*), (override));
    MOCK_METHOD(void, t2_uninit, (), (override));
    MOCK_METHOD(T2ERROR, t2_event_s, (const char*, const char*), (override));
    MOCK_METHOD(T2ERROR, t2_event_d, (const char*, int), (override));
    MOCK_METHOD(T2ERROR, t2_event_f, (char*, double), (override));
};

class BluetoothSdkAdapterTest : public ::testing::Test {
protected:
    NiceMock<SdkTelemetryMock> telemetry;
    NiceMock<MockManagerStub> manager;
    std::shared_ptr<NiceMock<MockAdapter>> adapter;
    std::unique_ptr<BtSdkAdapterImpl> sdkAdapter;

    void SetUp() override
    {
        TelemetryApi::setImpl(&telemetry);
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
        TelemetryApi::setImpl(nullptr);
    }

    std::string initAdapter()
    {
        return sdkAdapter->init(nullptr, BtEventCallbacks(), BtAuthCallbacks());
    }

    using DevicePtr = std::shared_ptr<NiceMock<MockDevice>>;

    static constexpr uint32_t kLoudspeakerClass = 0x0414;
    static constexpr uint32_t kSmartphoneClass  = 0x020C;

    static std::string handleOf(const std::string& mac)
    {
        return WPEFramework::Plugin::DeviceRegistry::deriveHandle(mac);
    }

    DevicePtr makeDevice(const std::string& mac, uint32_t classOfDevice)
    {
        auto device = std::make_shared<NiceMock<MockDevice>>();
        bluetooth::DeviceProperties props;
        props.classOfDevice = classOfDevice;
        ON_CALL(*device, address(_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(mac), ::testing::Return(Status{})));
        ON_CALL(*device, getAllProperties(_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(props), ::testing::Return(Status{})));
        ON_CALL(*device, state()).WillByDefault(::testing::Return(bluetooth::DeviceState::Connected));
        ON_CALL(*device, connect(_, _)).WillByDefault(::testing::Return(Status{}));
        ON_CALL(*device, disconnect(_, _)).WillByDefault(::testing::Return(Status{}));
        return device;
    }

    // Makes `all` the devices the adapter knows and `connected` the ones BlueZ reports as connected, then initialises.
    void initWithDevices(const std::vector<DevicePtr>& all, const std::vector<DevicePtr>& connected,
                         const BtEventCallbacks& callbacks = BtEventCallbacks(),
                         const BtAuthCallbacks& authCallbacks = BtAuthCallbacks())
    {
        const std::vector<std::shared_ptr<bluetooth::Device>> allDevices(all.begin(), all.end());
        const std::vector<std::shared_ptr<bluetooth::Device>> connectedDevices(connected.begin(), connected.end());
        ON_CALL(*adapter, getDevices()).WillByDefault(::testing::Return(std::move(allDevices)));
        ON_CALL(*adapter, getDevices(bluetooth::DeviceState::Connected)).WillByDefault(::testing::Return(std::move(connectedDevices)));
        ASSERT_EQ(std::string(""), sdkAdapter->init(nullptr, BtEventCallbacks(callbacks), BtAuthCallbacks(authCallbacks)));
    }

    // What the plugin keeps for the volume and mute (the device manager's store in production).
    struct VolumeStore {
        std::map<std::string, uint8_t> volume;
        bool hasMute = false;
        bool mute = false;
    };
    struct MediaStatus {
        long long deviceId;
        std::string deviceType;
        uint8_t volume;
        bool mute;
        std::string command;
    };
    struct AudioDevice {
        DevicePtr device;
        std::string mac;
        std::string handle;
        std::function<void(bluetooth::AudioEvent, bluetooth::AudioEventData)> audioCallback;
        std::function<void(bluetooth::DeviceEvent, std::shared_ptr<bluetooth::Device>)> deviceCallback;
    };

    VolumeStore store;
    std::vector<MediaStatus> statuses;

    std::shared_ptr<AudioDevice> makeAudioDevice(const std::string& mac, uint32_t classOfDevice = kLoudspeakerClass)
    {
        auto audio = std::make_shared<AudioDevice>();
        audio->device = makeDevice(mac, classOfDevice);
        audio->mac = mac;
        audio->handle = handleOf(mac);
        ON_CALL(*audio->device, registerForAudioEvents(_)).WillByDefault(::testing::SaveArg<0>(&audio->audioCallback));
        ON_CALL(*audio->device, registerForEvents(_)).WillByDefault(::testing::SaveArg<0>(&audio->deviceCallback));
        return audio;
    }

    // Initialises with the given devices, which are all connected, the store above and a recorder of the media status.
    void initAudioDevices(const std::vector<std::shared_ptr<AudioDevice>>& audioDevices)
    {
        std::vector<DevicePtr> devices;
        for (const auto& audio : audioDevices) devices.push_back(audio->device);

        BtEventCallbacks events;
        events.onDeviceMediaStatus = [this](long long int deviceId, const std::string&, const std::string& deviceType,
                                            uint8_t volume, bool mute, const std::string& command) {
            statuses.push_back({deviceId, deviceType, volume, mute, command});
        };
        BtAuthCallbacks auth;
        auth.getLastVolume = [this](const std::string& handle, uint8_t& volume) {
            const auto it = store.volume.find(handle);
            if (it == store.volume.end()) return false;
            volume = it->second;
            return true;
        };
        auth.setLastVolume = [this](const std::string& handle, uint8_t volume) { store.volume[handle] = volume; };
        auth.getLastMute = [this](bool& mute) {
            if (!store.hasMute) return false;
            mute = store.mute;
            return true;
        };
        auth.setLastMute = [this](bool mute) {
            store.hasMute = true;
            store.mute = mute;
        };
        initWithDevices(devices, devices, events, auth);
    }

    static bluetooth::AudioEventData audioData(const std::string& mac, float volume, bool muted)
    {
        bluetooth::AudioEventData data;
        data.deviceMacAddress = mac;
        data.volume = volume;
        data.muted = muted;
        data.delayCompensation = 0;
        return data;
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

TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_DisconnectsOtherAudioOutputFirst)
{
    auto target  = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    auto speaker = makeDevice("AA:BB:CC:DD:EE:02", kLoudspeakerClass);
    initWithDevices({target, speaker}, {speaker});

    ::testing::InSequence order;
    EXPECT_CALL(*speaker, disconnect(true, _)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*target, connect(true, _)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_OtherDisconnectFails_DoesNotConnect)
{
    auto target  = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    auto speaker = makeDevice("AA:BB:CC:DD:EE:02", kLoudspeakerClass);
    initWithDevices({target, speaker}, {speaker});

    EXPECT_CALL(*speaker, disconnect(true, _)).WillOnce(::testing::Return(kSdkFailure));
    EXPECT_CALL(*target, connect(_, _)).Times(0);

    EXPECT_FALSE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_LeavesConnectedSmartphoneAlone)
{
    auto target = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    auto phone  = makeDevice("AA:BB:CC:DD:EE:03", kSmartphoneClass);
    initWithDevices({target, phone}, {phone});

    EXPECT_CALL(*phone, disconnect(_, _)).Times(0);
    EXPECT_CALL(*target, connect(true, _)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_Smartphone_LeavesConnectedAudioOutputAlone)
{
    auto phone   = makeDevice("AA:BB:CC:DD:EE:03", kSmartphoneClass);
    auto speaker = makeDevice("AA:BB:CC:DD:EE:02", kLoudspeakerClass);
    initWithDevices({phone, speaker}, {speaker});

    EXPECT_CALL(*speaker, disconnect(_, _)).Times(0);
    EXPECT_CALL(*phone, connect(true, _)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:03"), "SMARTPHONE"));
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_AlreadyConnected_DisconnectsNothing)
{
    auto target = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    initWithDevices({target}, {target});

    EXPECT_CALL(*target, disconnect(_, _)).Times(0);
    EXPECT_CALL(*target, connect(true, _)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_ClientTypeIsIgnored)
{
    auto target  = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    auto speaker = makeDevice("AA:BB:CC:DD:EE:02", kLoudspeakerClass);
    initWithDevices({target, speaker}, {speaker});

    EXPECT_CALL(*speaker, disconnect(true, _)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*target, connect(true, _)).WillOnce(::testing::Return(Status{}));

    // The client's deviceType defaults to SMARTPHONE when it is omitted; the device's own type decides.
    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "SMARTPHONE"));
}

// BTMgr disconnects the other audio device while the connect runs: its CONNECTION_CHANGE is sent at once, before the
// connect response, and the new device's CONNECTION_CHANGE follows the response.
TEST_F(BluetoothSdkAdapterTest, connectDevice_AudioOutput_NotificationsFollowBtmgrOrder)
{
    auto target  = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    auto speaker = makeDevice("AA:BB:CC:DD:EE:02", kLoudspeakerClass);
    std::weak_ptr<bluetooth::Device> targetWeak  = target;
    std::weak_ptr<bluetooth::Device> speakerWeak = speaker;

    using DeviceCallback = std::function<void(bluetooth::DeviceEvent, std::shared_ptr<bluetooth::Device>)>;
    DeviceCallback targetCallback;
    DeviceCallback speakerCallback;
    ON_CALL(*target, registerForEvents(_)).WillByDefault(::testing::SaveArg<0>(&targetCallback));
    ON_CALL(*speaker, registerForEvents(_)).WillByDefault(::testing::SaveArg<0>(&speakerCallback));

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::string> log;
    auto record = [&](const std::string& entry) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            log.push_back(entry);
        }
        cv.notify_all();
    };
    auto waitForEntries = [&](size_t count) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(3), [&] { return log.size() >= count; });
    };

    BtEventCallbacks callbacks;
    callbacks.onStatusChanged = [&](const std::string&, const std::string& newStatus, const std::string& deviceId,
                                    const std::string&, const std::string&, uint32_t, uint16_t, bool,
                                    bool connected, bool, bool, bool) {
        record(newStatus + ":" + deviceId + ":" + (connected ? "connected" : "disconnected"));
    };
    initWithDevices({target, speaker}, {speaker}, callbacks);

    EXPECT_CALL(*speaker, disconnect(true, _))
        .WillOnce(::testing::Invoke([&](bool, uint8_t) {
            speakerCallback(bluetooth::DeviceEvent::Disconnected, speakerWeak.lock());
            return Status{};
        }));
    EXPECT_CALL(*target, connect(true, _))
        .WillOnce(::testing::Invoke([&](bool, uint8_t) {
            // A real connect takes a while, so the old device's change has been sent before the new one is raised.
            EXPECT_TRUE(waitForEntries(1));
            targetCallback(bluetooth::DeviceEvent::Connected, targetWeak.lock());
            return Status{};
        }));

    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
    record("response");
    ASSERT_TRUE(waitForEntries(3));

    std::lock_guard<std::mutex> lock(mutex);
    const std::vector<std::string> expected{
        "CONNECTION_CHANGE:" + handleOf("AA:BB:CC:DD:EE:02") + ":disconnected",
        "response",
        "CONNECTION_CHANGE:" + handleOf("AA:BB:CC:DD:EE:01") + ":connected"};
    EXPECT_EQ(expected, log);
}

// A failed pair or connect is reported only through the return value; the SDK backend sends no onRequestFailed, and
// only the synchronous calls are used, so a failure can never arrive later.
TEST_F(BluetoothSdkAdapterTest, pairDevice_SdkFailure_ReturnsFalseWithoutRequestFailedNotification)
{
    auto device = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    int requestFailed = 0;
    BtEventCallbacks callbacks;
    callbacks.onRequestFailed = [&](const std::string&, const std::string&, const std::string&, const std::string&,
                                    uint32_t, uint16_t, bool, bool, bool) { ++requestFailed; };
    initWithDevices({device}, {}, callbacks);

    EXPECT_CALL(*device, pair(true, _)).WillOnce(::testing::Return(kSdkFailure));
    EXPECT_CALL(*device, pair(false, _)).Times(0);

    EXPECT_FALSE(sdkAdapter->pairDevice(handleOf("AA:BB:CC:DD:EE:01")));

    // Longer than the delay before held notifications are released.
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_EQ(0, requestFailed);
}

TEST_F(BluetoothSdkAdapterTest, connectDevice_SdkFailure_ReturnsFalseWithoutRequestFailedNotification)
{
    auto device = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    int requestFailed = 0;
    BtEventCallbacks callbacks;
    callbacks.onRequestFailed = [&](const std::string&, const std::string&, const std::string&, const std::string&,
                                    uint32_t, uint16_t, bool, bool, bool) { ++requestFailed; };
    initWithDevices({device}, {}, callbacks);

    EXPECT_CALL(*device, connect(true, _)).WillOnce(::testing::Return(kSdkFailure));
    EXPECT_CALL(*device, connect(false, _)).Times(0);

    EXPECT_FALSE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_EQ(0, requestFailed);
}

TEST_F(BluetoothSdkAdapterTest, pairAndConnect_Success_UseTheSynchronousCalls)
{
    auto device = makeDevice("AA:BB:CC:DD:EE:01", kLoudspeakerClass);
    initWithDevices({device}, {});

    EXPECT_CALL(*device, pair(true, _)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*device, pair(false, _)).Times(0);
    EXPECT_CALL(*device, connect(true, _)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*device, connect(false, _)).Times(0);

    EXPECT_TRUE(sdkAdapter->pairDevice(handleOf("AA:BB:CC:DD:EE:01")));
    EXPECT_TRUE(sdkAdapter->connectDevice(handleOf("AA:BB:CC:DD:EE:01"), "LOUDSPEAKER"));
}

// ── Volume and mute ──────────────────────────────────────────────────────────────

TEST(VolumeUtilsTest, volumeScale_RoundTripsEveryValue)
{
    using namespace WPEFramework::Plugin;
    for (int volume = 0; volume <= 255; ++volume) {
        EXPECT_EQ(volume, volumeFromSdk(volumeToSdk(static_cast<uint8_t>(volume))));
    }
    EXPECT_FLOAT_EQ(0.0f, volumeToSdk(0));
    EXPECT_FLOAT_EQ(1.0f, volumeToSdk(255));
    EXPECT_EQ(0, volumeFromSdk(-0.5f));
    EXPECT_EQ(255, volumeFromSdk(1.5f));
    EXPECT_EQ(128, volumeFromSdk(0.5f));
}

TEST(VolumeUtilsTest, volumeSteps_FollowBtmgrLocalSteps)
{
    using namespace WPEFramework::Plugin;
    EXPECT_EQ(5, volumeStepUp(0));
    EXPECT_EQ(5, volumeStepUp(4));
    EXPECT_EQ(15, volumeStepUp(5));
    EXPECT_EQ(255, volumeStepUp(245));
    EXPECT_EQ(255, volumeStepUp(246));
    EXPECT_EQ(255, volumeStepUp(255));
    EXPECT_EQ(250, volumeStepDown(255));
    EXPECT_EQ(250, volumeStepDown(251));
    EXPECT_EQ(240, volumeStepDown(250));
    EXPECT_EQ(0, volumeStepDown(10));
    EXPECT_EQ(0, volumeStepDown(9));
    EXPECT_EQ(0, volumeStepDown(0));
}

TEST(VolumeUtilsTest, volumeOpType_MapsLikeTheLegacyPlugin)
{
    using namespace WPEFramework::Plugin;
    EXPECT_EQ(VolumeOpType::AudioOutput, volumeOpTypeFromDeviceType("LOUDSPEAKER"));
    EXPECT_EQ(VolumeOpType::AudioOutput, volumeOpTypeFromDeviceType("HEADPHONES"));
    EXPECT_EQ(VolumeOpType::AudioOutput, volumeOpTypeFromDeviceType("WEARABLE HEADSET"));
    EXPECT_EQ(VolumeOpType::AudioOutput, volumeOpTypeFromDeviceType("HIFI AUDIO DEVICE"));
    EXPECT_EQ(VolumeOpType::AudioInput, volumeOpTypeFromDeviceType("SMARTPHONE"));
    EXPECT_EQ(VolumeOpType::AudioInput, volumeOpTypeFromDeviceType("TABLET"));
    EXPECT_EQ(VolumeOpType::Hid, volumeOpTypeFromDeviceType("KEYBOARD"));
    EXPECT_EQ(VolumeOpType::Le, volumeOpTypeFromDeviceType("LE TILE"));
    EXPECT_EQ(VolumeOpType::Unknown, volumeOpTypeFromDeviceType("DEFAULT"));
    EXPECT_EQ(VolumeOpType::AudioOutput, volumeOpTypeFromDeviceType("SOMETHING ELSE"));
}

TEST(VolumeUtilsTest, mediaStatusCommand_MuteChangeComesBeforeVolumeChange)
{
    using WPEFramework::Plugin::mediaStatusCommand;
    EXPECT_STREQ("AUDIO_MUTE", mediaStatusCommand(100, false, 200, true));
    EXPECT_STREQ("AUDIO_UNMUTE", mediaStatusCommand(100, true, 50, false));
    EXPECT_STREQ("VOLUME_UP", mediaStatusCommand(100, false, 101, false));
    EXPECT_STREQ("VOLUME_DOWN", mediaStatusCommand(100, true, 99, true));
    EXPECT_STREQ("CMD_UNKNOWN", mediaStatusCommand(100, false, 100, false));
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_Success_SetsStoresAndNotifies)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_CALL(*speaker->device, setVolume(::testing::FloatEq(100.0f / 255.0f))).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*speaker->device, setMute(false)).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 100, false));

    EXPECT_EQ(100, store.volume[speaker->handle]);
    EXPECT_TRUE(store.hasMute);
    EXPECT_FALSE(store.mute);
    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(std::stoll(speaker->handle), statuses[0].deviceId);
    EXPECT_EQ(std::string("LOUDSPEAKER"), statuses[0].deviceType);
    EXPECT_EQ(100, statuses[0].volume);
    EXPECT_FALSE(statuses[0].mute);
    // Nothing is stored yet, so it is compared with BTMgr's starting volume of 254.
    EXPECT_EQ(std::string("VOLUME_DOWN"), statuses[0].command);
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_CommandIsDerivedFromTheStoredState)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 100;

    EXPECT_TRUE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 120, false));
    EXPECT_TRUE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 120, true));
    EXPECT_TRUE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 120, true));

    ASSERT_EQ(3u, statuses.size());
    EXPECT_EQ(std::string("VOLUME_UP"), statuses[0].command);
    EXPECT_EQ(std::string("AUDIO_MUTE"), statuses[1].command);
    EXPECT_EQ(std::string("CMD_UNKNOWN"), statuses[2].command);
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_NotAnAudioOutputType_FailsWithoutCallingTheSdk)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);
    EXPECT_CALL(*speaker->device, setMute(_)).Times(0);

    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "SMARTPHONE", 100, false));
    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "KEYBOARD", 100, false));
    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "DEFAULT", 100, false));
    EXPECT_TRUE(statuses.empty());
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_DeviceNotConnected_Fails)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    ON_CALL(*speaker->device, state()).WillByDefault(::testing::Return(bluetooth::DeviceState::Paired));

    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);

    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 100, false));
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_UnknownDevice_Fails)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({std::move(speaker)});

    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(9999, "LOUDSPEAKER", 100, false));
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_SdkVolumeFails_DoesNotSetMute)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_CALL(*speaker->device, setVolume(_)).WillOnce(::testing::Return(kSdkFailure));
    EXPECT_CALL(*speaker->device, setMute(_)).Times(0);

    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 100, false));
    EXPECT_TRUE(store.volume.empty());
    EXPECT_TRUE(statuses.empty());
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_SdkMuteFails_FailsWithoutNotification)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_CALL(*speaker->device, setVolume(_)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*speaker->device, setMute(true)).WillOnce(::testing::Return(kSdkFailure));

    EXPECT_FALSE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 100, true));
    // As BTMgr does, the volume that was applied is kept.
    EXPECT_EQ(100, store.volume[speaker->handle]);
    EXPECT_FALSE(store.hasMute);
    EXPECT_TRUE(statuses.empty());
}

TEST_F(BluetoothSdkAdapterTest, setDeviceVolumeMute_SdkEchoOfTheChange_IsNotNotifiedTwice)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    const std::string mac = speaker->mac;
    auto* audio = speaker.get();

    // The SDK reports the change from inside the call that makes it.
    EXPECT_CALL(*speaker->device, setVolume(_))
        .WillOnce(::testing::Invoke([&](float volume) {
            audio->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(mac, volume, false));
            return Status{};
        }));
    EXPECT_CALL(*speaker->device, setMute(true))
        .WillOnce(::testing::Invoke([&](bool mute) {
            audio->audioCallback(bluetooth::AudioEvent::MuteStateChanged, audioData(mac, 100.0f / 255.0f, mute));
            return Status{};
        }));

    EXPECT_TRUE(sdkAdapter->setDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER", 100, true));

    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(100, statuses[0].volume);
    EXPECT_TRUE(statuses[0].mute);
}

TEST_F(BluetoothSdkAdapterTest, getDeviceVolumeMute_NothingStored_ReadsTheSdk)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    ON_CALL(*speaker->device, getVolume()).WillByDefault(::testing::Return(100.0f / 255.0f));
    ON_CALL(*speaker->device, isMuted()).WillByDefault(::testing::Return(true));

    const auto info = sdkAdapter->getDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER");

    EXPECT_TRUE(info.valid);
    EXPECT_EQ(100, info.volume);
    EXPECT_TRUE(info.mute);
}

TEST_F(BluetoothSdkAdapterTest, getDeviceVolumeMute_StoredValuesWin)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    ON_CALL(*speaker->device, getVolume()).WillByDefault(::testing::Return(100.0f / 255.0f));
    ON_CALL(*speaker->device, isMuted()).WillByDefault(::testing::Return(true));
    store.volume[speaker->handle] = 50;
    store.hasMute = true;
    store.mute = false;

    const auto info = sdkAdapter->getDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER");

    EXPECT_TRUE(info.valid);
    EXPECT_EQ(50, info.volume);
    EXPECT_FALSE(info.mute);
}

TEST_F(BluetoothSdkAdapterTest, getDeviceVolumeMute_NotAConnectedAudioOutputDevice_IsInvalid)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_FALSE(sdkAdapter->getDeviceVolumeMute(std::stoll(speaker->handle), "SMARTPHONE").valid);
    EXPECT_FALSE(sdkAdapter->getDeviceVolumeMute(9999, "LOUDSPEAKER").valid);

    ON_CALL(*speaker->device, state()).WillByDefault(::testing::Return(bluetooth::DeviceState::Paired));
    EXPECT_FALSE(sdkAdapter->getDeviceVolumeMute(std::stoll(speaker->handle), "LOUDSPEAKER").valid);
}

TEST_F(BluetoothSdkAdapterTest, setAudioControlCommand_VolumeUp_StepsStoresAndNotifies)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 3;

    EXPECT_CALL(*speaker->device, setVolume(::testing::FloatEq(5.0f / 255.0f))).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "VOLUME_UP"));

    EXPECT_EQ(5, store.volume[speaker->handle]);
    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(5, statuses[0].volume);
    EXPECT_FALSE(statuses[0].mute);
    EXPECT_EQ(std::string("VOLUME_UP"), statuses[0].command);
}

TEST_F(BluetoothSdkAdapterTest, setAudioControlCommand_VolumeDownToZero_ReportsMute)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    ON_CALL(*speaker->device, getVolume()).WillByDefault(::testing::Return(7.0f / 255.0f));

    EXPECT_CALL(*speaker->device, setVolume(::testing::FloatEq(0.0f))).WillOnce(::testing::Return(Status{}));

    EXPECT_TRUE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "VOLUME_DOWN"));

    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(0, statuses[0].volume);
    EXPECT_TRUE(statuses[0].mute);
    EXPECT_EQ(std::string("VOLUME_DOWN"), statuses[0].command);
}

TEST_F(BluetoothSdkAdapterTest, setAudioControlCommand_MuteAndUnmute_SetMuteAndKeepVolume)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 90;

    EXPECT_CALL(*speaker->device, setMute(true)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*speaker->device, setMute(false)).WillOnce(::testing::Return(Status{}));
    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);

    EXPECT_TRUE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "AUDIO_MUTE"));
    EXPECT_TRUE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "AUDIO_UNMUTE"));

    ASSERT_EQ(2u, statuses.size());
    EXPECT_EQ(std::string("AUDIO_MUTE"), statuses[0].command);
    EXPECT_TRUE(statuses[0].mute);
    EXPECT_EQ(90, statuses[0].volume);
    EXPECT_EQ(std::string("AUDIO_UNMUTE"), statuses[1].command);
    EXPECT_FALSE(statuses[1].mute);
    EXPECT_FALSE(store.mute);
}

TEST_F(BluetoothSdkAdapterTest, setAudioControlCommand_OtherCommandsAndDevices_Fail)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    auto phone = makeAudioDevice("AA:BB:CC:DD:EE:03", kSmartphoneClass);
    initAudioDevices({speaker, phone});

    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);
    EXPECT_CALL(*speaker->device, setMute(_)).Times(0);
    EXPECT_CALL(*phone->device, setVolume(_)).Times(0);

    EXPECT_FALSE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "PAUSE"));
    EXPECT_FALSE(sdkAdapter->setAudioControlCommand(std::stoll(speaker->handle), "SKIP_NEXT"));
    EXPECT_FALSE(sdkAdapter->setAudioControlCommand(std::stoll(phone->handle), "VOLUME_UP"));
    EXPECT_FALSE(sdkAdapter->setAudioControlCommand(9999, "VOLUME_UP"));
    EXPECT_TRUE(statuses.empty());
}

TEST_F(BluetoothSdkAdapterTest, audioEvent_VolumeChangedByTheDevice_IsStoredAndNotified)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 200;

    speaker->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(speaker->mac, 0.5f, false));

    EXPECT_EQ(128, store.volume[speaker->handle]);
    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(std::stoll(speaker->handle), statuses[0].deviceId);
    EXPECT_EQ(128, statuses[0].volume);
    EXPECT_FALSE(statuses[0].mute);
    EXPECT_EQ(std::string("VOLUME_DOWN"), statuses[0].command);
}

TEST_F(BluetoothSdkAdapterTest, audioEvent_MuteChangedByThePlatform_IsStoredAndNotified)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    speaker->audioCallback(bluetooth::AudioEvent::MuteStateChanged, audioData(speaker->mac, 0.5f, true));

    EXPECT_TRUE(store.hasMute);
    EXPECT_TRUE(store.mute);
    ASSERT_EQ(1u, statuses.size());
    EXPECT_TRUE(statuses[0].mute);
    EXPECT_EQ(std::string("AUDIO_MUTE"), statuses[0].command);
}

TEST_F(BluetoothSdkAdapterTest, audioEvent_DelayChangeAndNonAudioOutputDevice_AreIgnored)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    auto phone = makeAudioDevice("AA:BB:CC:DD:EE:03", kSmartphoneClass);
    initAudioDevices({speaker, phone});

    speaker->audioCallback(bluetooth::AudioEvent::DelayCompensationChanged, audioData(speaker->mac, 0.5f, false));
    phone->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(phone->mac, 0.5f, false));

    EXPECT_TRUE(statuses.empty());
    EXPECT_TRUE(store.volume.empty());
}

TEST_F(BluetoothSdkAdapterTest, deviceConnects_StoredVolumeIsAppliedOnceItsAudioIsAvailable)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 120;
    ON_CALL(*speaker->device, getVolume()).WillByDefault(::testing::Return(50.0f / 255.0f));

    speaker->deviceCallback(bluetooth::DeviceEvent::Connected, speaker->device);
    // Nothing is applied before the device's audio exists.
    EXPECT_TRUE(statuses.empty());

    EXPECT_CALL(*speaker->device, setVolume(::testing::FloatEq(120.0f / 255.0f))).WillOnce(::testing::Return(Status{}));
    speaker->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(speaker->mac, 0.9f, false));

    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(120, statuses[0].volume);
    EXPECT_EQ(std::string("VOLUME_UP"), statuses[0].command);
    // The device's own value did not replace the stored one.
    EXPECT_EQ(120, store.volume[speaker->handle]);

    // The events the apply causes are not reported for the next 4 seconds.
    speaker->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(speaker->mac, 120.0f / 255.0f, false));
    EXPECT_EQ(1u, statuses.size());
}

TEST_F(BluetoothSdkAdapterTest, deviceConnects_NothingStored_LeavesTheVolumeAlone)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});

    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);
    speaker->deviceCallback(bluetooth::DeviceEvent::Connected, speaker->device);
    speaker->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(speaker->mac, 0.5f, false));

    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(128, statuses[0].volume);
}

TEST_F(BluetoothSdkAdapterTest, deviceDisconnects_PendingRestoreIsDropped)
{
    auto speaker = makeAudioDevice("AA:BB:CC:DD:EE:02");
    initAudioDevices({speaker});
    store.volume[speaker->handle] = 120;

    speaker->deviceCallback(bluetooth::DeviceEvent::Connected, speaker->device);
    speaker->deviceCallback(bluetooth::DeviceEvent::Disconnected, speaker->device);

    EXPECT_CALL(*speaker->device, setVolume(_)).Times(0);
    speaker->audioCallback(bluetooth::AudioEvent::VolumeChanged, audioData(speaker->mac, 0.5f, false));

    ASSERT_EQ(1u, statuses.size());
    EXPECT_EQ(128, statuses[0].volume);
}

TEST_F(BluetoothSdkAdapterTest, setAudioStream_AcceptsOnlyPrimary)
{
    ASSERT_EQ(std::string(""), initAdapter());

    EXPECT_TRUE(sdkAdapter->setAudioStream(0, "PRIMARY"));
    EXPECT_TRUE(sdkAdapter->setAudioStream(0, "primary"));
    EXPECT_FALSE(sdkAdapter->setAudioStream(0, "AUXILIARY"));
    EXPECT_FALSE(sdkAdapter->setAudioStream(0, "SOMETHING"));
    EXPECT_FALSE(sdkAdapter->setAudioStream(0, ""));
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
