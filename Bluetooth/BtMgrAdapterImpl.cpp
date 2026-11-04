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

#include "Module.h"
#include "BtMgrAdapterImpl.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>

#include "btmgr.h"
#include <UtilsIarm.h>
#include <UtilsLogging.h>
#include <UtilsString.h>

namespace {
// IARM client name used when registering with BTMgr.
// Matches Utils::IARM::NAME from entservices-helpers without pulling in libIBus.h.
constexpr const char* kIarmClientName = "Thunder_Plugins";
} // namespace

namespace WPEFramework {
namespace Plugin {

namespace {

// Status / event ID strings matching the existing JSON-RPC contract.
constexpr const char* STATUS_DISCOVERY_STARTED   = "DISCOVERY_STARTED";
constexpr const char* STATUS_DISCOVERY_COMPLETED = "DISCOVERY_COMPLETED";
constexpr const char* STATUS_PAIRING_CHANGE      = "PAIRING_CHANGE";
constexpr const char* STATUS_CONNECTION_CHANGE   = "CONNECTION_CHANGE";
constexpr const char* STATUS_PAIRING_FAILED      = "PAIRING_FAILED";
constexpr const char* STATUS_CONNECTION_FAILED   = "CONNECTION_FAILED";
constexpr const char* EVT_STATUS_CHANGED         = "onStatusChanged";
constexpr const char* EVT_REQUEST_FAILED         = "onRequestFailed";
constexpr const char* EVT_PAIRING_REQUEST        = "onPairingRequest";
constexpr const char* EVT_CONNECTION_REQUEST     = "onConnectionRequest";
constexpr const char* EVT_PLAYBACK_REQUEST       = "onPlaybackRequest";

bool parseHandle(const std::string& handleStr, BTRMgrDeviceHandle& handle) {
    try {
        handle = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    }
    catch (...) {
        return false;
    }
    return true;
}

// BTMgr reports the count separately from the fixed array, so clamp it before indexing.
size_t profileCount(const BTRMGR_DeviceServiceList_t& svc) {
    return std::min<size_t>(svc.m_numOfService, std::size(svc.m_profileInfo));
}

std::string joinProfiles(const BTRMGR_DeviceServiceList_t& svc) {
    std::string profiles;
    for (size_t i = 0; i < profileCount(svc); ++i) {
        if (!profiles.empty()) profiles += ";";
        profiles += svc.m_profileInfo[i].m_profile;
    }
    return profiles;
}

// Tracks event-thread dispatches in flight so deinit() can wait for them to drain.
std::mutex              s_inflightMutex;
std::condition_variable s_inflightCv;
int                     s_inflightCount = 0;

class InflightGuard {
public:
    InflightGuard() {
        std::lock_guard<std::mutex> lk(s_inflightMutex);
        ++s_inflightCount;
    }
    ~InflightGuard() {
        std::lock_guard<std::mutex> lk(s_inflightMutex);
        if (--s_inflightCount == 0) s_inflightCv.notify_all();
    }
    InflightGuard(const InflightGuard&) = delete;
    InflightGuard& operator=(const InflightGuard&) = delete;
};

} // namespace

// ── Static instance pointer for IARM callback ────────────────────────────────

std::atomic<BtMgrAdapterImpl*> BtMgrAdapterImpl::s_instance{nullptr};

// ── Lifecycle ────────────────────────────────────────────────────────────────

std::string BtMgrAdapterImpl::init(PluginHost::IShell* /* service */,
                                    BtEventCallbacks&& eventCallbacks,
                                    BtAuthCallbacks&& authCallbacks) {
    m_evtCbs  = std::move(eventCallbacks);
    m_authCbs = std::move(authCallbacks);
    s_instance = this;

    if (!Utils::IARM::init()) {
        LOGERR("Utils::IARM::init() failed; continuing");
    }

    BTRMGR_Result_t rc = BTRMGR_RegisterForCallbacks(kIarmClientName);
    if (rc != BTRMGR_RESULT_SUCCESS) {
        s_instance = nullptr;
        return std::string("BTRMGR_RegisterForCallbacks failed: ") + std::to_string(rc);
    }

    rc = BTRMGR_RegisterEventCallback(
        // coverity[pass_by_value] - BTRMGR_EventCallback ABI requires an event value.
        [](BTRMGR_EventMessage_t eventMsg) -> BTRMGR_Result_t {
            // Count the dispatch before loading the pointer so deinit() cannot miss it.
            InflightGuard guard;
            BtMgrAdapterImpl* inst = s_instance.load();
            if (inst) {
                 inst->onEvent(&eventMsg, sizeof(eventMsg));
                 return BTRMGR_RESULT_SUCCESS;
             }
             return BTRMGR_RESULT_INIT_FAILED;
        });
    if (rc != BTRMGR_RESULT_SUCCESS) {
        s_instance = nullptr;
        BTRMGR_UnRegisterFromCallbacks(kIarmClientName);
        return std::string("BTRMGR_RegisterEventCallback failed: ") + std::to_string(rc);
    }

    return "";
}

void BtMgrAdapterImpl::deinit() {
    s_instance = nullptr;
    BTRMGR_UnRegisterFromCallbacks(kIarmClientName);
}

void BtSdkAdapterImpl::autoConnectWorker() {
    // Must not be called from the BTRMGR event thread, or this wait never completes.
    std::unique_lock<std::mutex> lk(s_inflightMutex);
    while (s_inflightCount != 0) {
        s_inflightCv.wait(lk);
    }
}

// ── Adapter operations ────────────────────────────────────────────────────────

bool BtMgrAdapterImpl::getAdapterPowered(bool& powered) const {
    unsigned char power = 0;
    BTRMGR_Result_t rc = BTRMGR_GetAdapterPowerStatus(0, &power);
    if (rc == BTRMGR_RESULT_SUCCESS) powered = (power != 0);
    return rc == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::setAdapterPowered(bool powered) {
    return BTRMGR_SetAdapterPowerStatus(0, powered ? 1 : 0) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::getAdapterName(std::string& name) const {
    char buf[BTRMGR_NAME_LEN_MAX] = {};
    if (BTRMGR_GetAdapterName(0, buf) != BTRMGR_RESULT_SUCCESS) return false;
    name = buf;
    return true;
}

bool BtMgrAdapterImpl::setAdapterName(const std::string& name) {
    return BTRMGR_SetAdapterName(0, name.c_str()) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::isAdapterDiscoverable(bool& discoverable) const {
    unsigned char d = 0;
    if (BTRMGR_IsAdapterDiscoverable(0, &d) != BTRMGR_RESULT_SUCCESS) return false;
    discoverable = (d != 0);
    return true;
}

bool BtMgrAdapterImpl::setAdapterDiscoverable(bool discoverable, int timeoutSeconds) {
    return BTRMGR_SetAdapterDiscoverable(0, discoverable ? 1 : 0,
                                         timeoutSeconds) == BTRMGR_RESULT_SUCCESS;
}

// ── Discovery ────────────────────────────────────────────────────────────────

bool BtMgrAdapterImpl::startScan(const std::string& profile) {
    // Matches legacy startDeviceDiscovery(): success reflects adapter presence,
    // not the BTRMGR_StartDeviceDiscovery call result (which is only logged).
    unsigned char numOfAdapters = 0;
    if (BTRMGR_GetNumberOfAdapters(&numOfAdapters) != BTRMGR_RESULT_SUCCESS) {
        LOGERR("Failed to get the number of adapters..!");
    }
    if (numOfAdapters == 0) {
        return false;
    }

    // The legacy plugin defaulted an unspecified profile to audio devices and gamepads.
    static const std::string kLegacyDefaultProfile =
        "LOUDSPEAKER, HEADPHONES, WEARABLE HEADSET, HIFI AUDIO DEVICE, KEYBOARD, MOUSE, JOYSTICK";
    BTRMGR_DeviceOperationType_t opType = static_cast<BTRMGR_DeviceOperationType_t>(
        deviceOpTypeFromProfile(profile.empty() ? kLegacyDefaultProfile : profile));
    if (BTRMGR_StartDeviceDiscovery(0, opType) != BTRMGR_RESULT_SUCCESS) {
        LOGERR("Failed to start the discovery..!");
    }
    return true;
}

bool BtMgrAdapterImpl::stopScan() {
    // Intentionally unchanged: legacy alignment - the BTMgr-based plugin always stopped discovery with AUDIO_OUTPUT
    // regardless of the opType used to start it.
    return BTRMGR_StopDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) == BTRMGR_RESULT_SUCCESS;
}

// ── Device lists ──────────────────────────────────────────────────────────────

template <typename T>
static IBtAdapter::BtDeviceInfo deviceInfoFromBtmgr(const T& d, bool isPaired) {
    IBtAdapter::BtDeviceInfo info;
    info.mac        = d.m_deviceAddress;
    info.handleStr  = std::to_string(d.m_deviceHandle);
    info.name       = d.m_name;
    const char* dt  = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
    info.deviceType = dt ? dt : "UNKNOWN";
    info.isGamePad  = (d.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD);
    info.connected  = d.m_isConnected  != 0;
    info.paired     = isPaired;
    info.classOfDevice = d.m_ui32DevClassBtSpec;
    info.appearance    = static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec);
    return info;
}

std::vector<IBtAdapter::BtDeviceInfo> BtMgrAdapterImpl::getDiscoveredDevices() const {
    std::vector<BtDeviceInfo> result;
    BTRMGR_DiscoveredDevicesList_t list;
    memset(&list, 0, sizeof(list));
    if (BTRMGR_GetDiscoveredDevices(0, &list) != BTRMGR_RESULT_SUCCESS) return result;
    const size_t count = std::min<size_t>(list.m_numOfDevices, std::size(list.m_deviceProperty));
    for (size_t i = 0; i < count; ++i) {
        const auto& d = list.m_deviceProperty[i];
        auto info = deviceInfoFromBtmgr(d, d.m_isPairedDevice != 0);
        cacheHandleToMac(info.handleStr, info.mac);
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<IBtAdapter::BtDeviceInfo> BtMgrAdapterImpl::getPairedDevices() const {
    std::vector<BtDeviceInfo> result;
    queryPairedDevices(result);
    return result;
}

bool BtMgrAdapterImpl::queryPairedDevices(std::vector<BtDeviceInfo>& out) const {
    out.clear();
    BTRMGR_PairedDevicesList_t list;
    memset(&list, 0, sizeof(list));
    if (BTRMGR_GetPairedDevices(0, &list) != BTRMGR_RESULT_SUCCESS) return false;
    const size_t count = std::min<size_t>(list.m_numOfDevices, std::size(list.m_deviceProperty));
    for (size_t i = 0; i < count; ++i) {
        auto info = deviceInfoFromBtmgr(list.m_deviceProperty[i], true);
        cacheHandleToMac(info.handleStr, info.mac);
        out.push_back(std::move(info));
    }
    return true;
}

std::vector<IBtAdapter::BtDeviceInfo> BtMgrAdapterImpl::getConnectedDevices() const {
    std::vector<BtDeviceInfo> result;
    BTRMGR_ConnectedDevicesList_t list;
    memset(&list, 0, sizeof(list));
    if (BTRMGR_GetConnectedDevices(0, &list) != BTRMGR_RESULT_SUCCESS) return result;
    const size_t count = std::min<size_t>(list.m_numOfDevices, std::size(list.m_deviceProperty));
    for (size_t i = 0; i < count; ++i) {
        const auto& d = list.m_deviceProperty[i];
        auto info = deviceInfoFromBtmgr(d, true);
        info.powerStatus = static_cast<uint32_t>(d.m_powerStatus);
        cacheHandleToMac(info.handleStr, info.mac);
        result.push_back(std::move(info));
    }
    return result;
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtMgrAdapterImpl::pairDevice(const std::string& handleStr) {
    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        return false;
    }

    return BTRMGR_PairDevice(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::unpairDevice(const std::string& handleStr) {
    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        LOGERR("Invalid device handle '%s'", handleStr.c_str());
        return false;
    }
    return BTRMGR_UnpairDevice(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::connectDevice(const std::string& handleStr, const std::string& deviceType) {
    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        LOGERR("Invalid device handle '%s'", handleStr.c_str());
        return false;
    }
    if (Utils::String::equal(deviceType, "LE TILE")) {
        return BTRMGR_ConnectToDevice(0, h, BTRMGR_DEVICE_OP_TYPE_LE) == BTRMGR_RESULT_SUCCESS;
    }
    if (Utils::String::equal(deviceType, "HUMAN INTERFACE DEVICE") ||
        Utils::String::contains(deviceType, "KEYBOARD") ||
        Utils::String::contains(deviceType, "MOUSE") ||
        Utils::String::contains(deviceType, "JOYSTICK")) {
        return BTRMGR_ConnectToDevice(0, h, BTRMGR_DEVICE_OP_TYPE_HID) == BTRMGR_RESULT_SUCCESS;
    }
    if (Utils::String::equal(deviceType, "SMARTPHONE") || Utils::String::equal(deviceType, "TABLET")) {
        return BTRMGR_StartAudioStreamingIn(0, h, BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) == BTRMGR_RESULT_SUCCESS;
    }
    return BTRMGR_StartAudioStreamingOut(0, h, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& deviceType) {
    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        LOGERR("Invalid device handle '%s'", handleStr.c_str());
        return false;
    }
    if (Utils::String::equal(deviceType, "LE TILE") ||
        Utils::String::equal(deviceType, "HUMAN INTERFACE DEVICE") ||
        Utils::String::contains(deviceType, "KEYBOARD") ||
        Utils::String::contains(deviceType, "MOUSE") ||
        Utils::String::contains(deviceType, "JOYSTICK")) {
        return BTRMGR_DisconnectFromDevice(0, h) == BTRMGR_RESULT_SUCCESS;
    }
    if (Utils::String::equal(deviceType, "SMARTPHONE") || Utils::String::equal(deviceType, "TABLET")) {
        return BTRMGR_StopAudioStreamingIn(0, h) == BTRMGR_RESULT_SUCCESS;
    }
    return BTRMGR_StopAudioStreamingOut(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                             BtDeviceProperties& props) const {
    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        LOGERR("Invalid device handle '%s'", handleStr.c_str());
        return false;
    }
    BTRMGR_DevicesProperty_t p;
    memset(&p, 0, sizeof(p));
    if (BTRMGR_GetDeviceProperties(0, h, &p) != BTRMGR_RESULT_SUCCESS) return false;

    props.handleStr    = handleStr;
    props.mac          = p.m_deviceAddress;
    props.name         = p.m_name;
    const char* dt     = BTRMGR_GetDeviceTypeAsString(p.m_deviceType);
    props.deviceType   = dt ? dt : "UNKNOWN";
    props.isGamePad    = (p.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD);
    props.rssi         = static_cast<int16_t>(p.m_rssi);
    props.signalLevel  = static_cast<int16_t>(p.m_signalLevel);
    props.batteryLevel = p.m_batteryLevel;
    props.vendorId     = static_cast<uint16_t>(p.m_vendorID);
    props.modalias     = p.m_modalias;
    props.firmwareRevision = p.m_firmwareRevision;

    for (size_t i = 0; i < profileCount(p.m_serviceInfo); ++i) {
        props.uuids.push_back(p.m_serviceInfo.m_profileInfo[i].m_profile);
    }

    cacheHandleToMac(handleStr, props.mac);
    return true;
}

std::string BtMgrAdapterImpl::getMacForHandle(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lk(m_mapMutex);
    auto it = m_handleToMac.find(handleStr);
    return (it != m_handleToMac.end()) ? it->second : "";
}

bool BtMgrAdapterImpl::respondToEvent(const std::string& handleStr, const std::string& eventType, bool accepted) {
    // Stateless by design (matches legacy setEventResponse): handleStr + eventType
    // identify the BTRMGR event directly, no server-side "pending" bookkeeping.
    BTRMGR_EventResponse_t rsp;
    memset(&rsp, 0, sizeof(rsp));

    BTRMgrDeviceHandle h = 0;
    if (!parseHandle(handleStr, h)) {
        LOGERR("Invalid device handle '%s'", handleStr.c_str());
        return false;
    }
    rsp.m_deviceHandle = h;

    if (eventType == EVT_PAIRING_REQUEST) {
        rsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST;
    } else if (eventType == EVT_CONNECTION_REQUEST) {
        rsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST;
    } else if (eventType == EVT_PLAYBACK_REQUEST) {
        rsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST;
    } else {
        LOGERR("Unrecognised event type '%s'", eventType.c_str());
        return false;
    }
    rsp.m_eventResp = accepted ? 1 : 0;

    return BTRMGR_SetEventResponse(0, &rsp) == BTRMGR_RESULT_SUCCESS;
}

// ── Audio operations ──────────────────────────────────────────────────────────

bool BtMgrAdapterImpl::setAudioStream(long long int /* deviceID */,
                                       const std::string& streamName) {
    BTRMGR_StreamOut_Type_t t = BTRMGR_STREAM_PRIMARY;
    if (Utils::String::equal(streamName, "AUXILIARY")) t = BTRMGR_STREAM_AUXILIARY;
    return BTRMGR_SetAudioStreamingOutType(0, t) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::setAudioControlCommand(long long int deviceID,
                                               const std::string& cmd) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(deviceID);

    if (cmd == "PLAY")          return BTRMGR_StartAudioStreamingIn(0, h,
                                           BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) == BTRMGR_RESULT_SUCCESS;
    if (cmd == "PAUSE")         return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_PAUSE)     == BTRMGR_RESULT_SUCCESS;
    if (cmd == "RESUME")        return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_PLAY)      == BTRMGR_RESULT_SUCCESS;
    if (cmd == "STOP")          return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_STOP)      == BTRMGR_RESULT_SUCCESS;
    if (cmd == "SKIP_NEXT")     return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_NEXT)      == BTRMGR_RESULT_SUCCESS;
    if (cmd == "SKIP_PREV")     return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_PREVIOUS)  == BTRMGR_RESULT_SUCCESS;
    if (cmd == "RESTART") {
        LOGERR("setAudioControlCommand: RESTART is not implemented");
        return false;
    }
    if (cmd == "AUDIO_MUTE")    return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_MUTE)      == BTRMGR_RESULT_SUCCESS;
    if (cmd == "AUDIO_UNMUTE")  return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_UNMUTE)    == BTRMGR_RESULT_SUCCESS;
    if (cmd == "VOLUME_UP")     return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_VOLUMEUP)  == BTRMGR_RESULT_SUCCESS;
    if (cmd == "VOLUME_DOWN")   return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_VOLUMEDOWN) == BTRMGR_RESULT_SUCCESS;
    LOGERR("setAudioControlCommand: unknown command '%s'", cmd.c_str());
    return false;
}

bool BtMgrAdapterImpl::setDeviceVolumeMute(long long int deviceID,
                                             const std::string& profile,
                                             uint8_t volume, bool mute) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(deviceID);
    BTRMGR_DeviceOperationType_t opType =
        static_cast<BTRMGR_DeviceOperationType_t>(deviceOpTypeFromProfile(profile));
    return BTRMGR_SetDeviceVolumeMute(0, h, opType, volume, mute ? 1 : 0) == BTRMGR_RESULT_SUCCESS;
}

IBtAdapter::BtDeviceVolumeMute BtMgrAdapterImpl::getDeviceVolumeMute(long long int deviceID,
                                                                       const std::string& profile) const {
    BtDeviceVolumeMute result;
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(deviceID);
    BTRMGR_DeviceOperationType_t opType =
        static_cast<BTRMGR_DeviceOperationType_t>(deviceOpTypeFromProfile(profile));
    unsigned char vol = 0, mute = 0;
    if (BTRMGR_GetDeviceVolumeMute(0, h, opType, &vol, &mute) == BTRMGR_RESULT_SUCCESS) {
        result.volume = vol;
        result.mute   = (mute != 0);
        result.valid  = true;
    }
    return result;
}

IBtAdapter::BtMediaTrackInfo BtMgrAdapterImpl::getMediaTrackInfo(long long int deviceID) const {
    BtMediaTrackInfo result;
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(deviceID);
    BTRMGR_MediaTrackInfo_t t;
    memset(&t, 0, sizeof(t));
    if (BTRMGR_GetMediaTrackInfo(0, h, &t) != BTRMGR_RESULT_SUCCESS) return result;
    result.album          = t.pcAlbum;
    result.genre          = t.pcGenre;
    result.title          = t.pcTitle;
    result.artist         = t.pcArtist;
    result.duration       = t.ui32Duration;
    result.trackNumber    = t.ui32TrackNumber;
    result.numberOfTracks = t.ui32NumberOfTracks;
    return result;
}

// ── Event callback ────────────────────────────────────────────────────────────

void BtMgrAdapterImpl::onEvent(void* data, size_t /*len*/) {
    if (!data) return;
    const auto& msg = *static_cast<const BTRMGR_EventMessage_t*>(data);

    switch (msg.m_eventType) {

    case BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED:
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged(EVT_STATUS_CHANGED, STATUS_DISCOVERY_STARTED,
                                     "", "", "", 0, 0, false, false, false, false, false);
        break;

    case BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE:
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged(EVT_STATUS_CHANGED, STATUS_DISCOVERY_COMPLETED,
                                     "", "", "", 0, 0, false, false, false, false, false);
        break;

    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE: {
        const auto& d = msg.m_discoveredDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        cacheHandleToMac(handleStr, d.m_deviceAddress);
        if (m_evtCbs.onDiscoveredDevice)
            m_evtCbs.onDiscoveredDevice(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                        d.m_ui32DevClassBtSpec,
                                        static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                        d.m_isPairedDevice != 0,
                                        d.m_isLastConnectedDevice != 0,
                                        d.m_isDiscovered ? "DISCOVERED" : "LOST");
        break;
    }

    case BTRMGR_EVENT_DEVICE_FOUND: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        cacheHandleToMac(handleStr, d.m_deviceAddress);
        if (m_evtCbs.onDeviceFound)
            m_evtCbs.onDeviceFound(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                   d.m_ui32DevClassBtSpec,
                                   static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                   d.m_isLastConnectedDevice != 0);
        break;
    }

    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        if (m_evtCbs.onDeviceLost)
            m_evtCbs.onDeviceLost(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                  d.m_ui32DevClassBtSpec,
                                  static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                  d.m_isLastConnectedDevice != 0);
        break;
    }

    case BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE: {
        const auto& d = msg.m_discoveredDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        bool ac = false;
        bool hasAC = m_evtCbs.getAutoConnect && m_evtCbs.getAutoConnect(handleStr, ac);
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged(EVT_STATUS_CHANGED, STATUS_PAIRING_CHANGE,
                                     handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     d.m_isPairedDevice != 0,
                                     d.m_isConnected != 0,
                                     d.m_isLastConnectedDevice != 0,
                                     hasAC, ac);
        break;
    }

    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged(EVT_STATUS_CHANGED, STATUS_PAIRING_CHANGE,
                                     handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     false, d.m_isConnected != 0,
                                     d.m_isLastConnectedDevice != 0, false, false);
        break;
    }

    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        bool ac = false;
        bool hasAC = m_evtCbs.getAutoConnect && m_evtCbs.getAutoConnect(handleStr, ac);
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged(EVT_STATUS_CHANGED, STATUS_CONNECTION_CHANGE,
                                     handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     true, d.m_isConnected != 0,
                                     d.m_isLastConnectedDevice != 0,
                                     hasAC, ac);
        break;
    }

    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED: {
        const auto& d = msg.m_discoveredDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        if (m_evtCbs.onRequestFailed)
            m_evtCbs.onRequestFailed(STATUS_PAIRING_FAILED, handleStr, d.m_name,
                                     dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     d.m_isPairedDevice != 0, d.m_isConnected != 0);
        break;
    }

    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        if (m_evtCbs.onRequestFailed)
            m_evtCbs.onRequestFailed(STATUS_PAIRING_FAILED, handleStr, d.m_name,
                                     dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     true, d.m_isConnected != 0);
        break;
    }

    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED: {
        const auto& d = msg.m_pairedDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        if (m_evtCbs.onRequestFailed)
            m_evtCbs.onRequestFailed(STATUS_CONNECTION_FAILED, handleStr, d.m_name,
                                     dt ? dt : "UNKNOWN",
                                     d.m_ui32DevClassBtSpec,
                                     static_cast<uint16_t>(d.m_ui16DevAppearanceBleSpec),
                                     true, d.m_isConnected != 0);
        break;
    }

    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST: {
        const auto& d = msg.m_externalDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        const std::string profiles = joinProfiles(d.m_serviceInfo);
        cacheHandleToMac(handleStr, d.m_deviceAddress);
        if (m_authCbs.onPairingRequest)
            m_authCbs.onPairingRequest(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                       d.m_vendorID, d.m_deviceAddress, profiles,
                                       d.m_externalDevicePIN != 0,
                                       d.m_externalDevicePIN);
        break;
    }

    case BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST: {
        const auto& d = msg.m_externalDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        const std::string profiles = joinProfiles(d.m_serviceInfo);
        cacheHandleToMac(handleStr, d.m_deviceAddress);

        // Once migrated, respond directly (accept or reject) and suppress the client notification either way.
        bool accept = false;
        if (m_authCbs.getConnectAutoResponse && m_authCbs.getConnectAutoResponse(handleStr, accept)) {
            const bool responded = respondToEvent(handleStr, EVT_CONNECTION_REQUEST, accept);
            if (!responded) {
                LOGERR("Failed to auto-respond to connection request for %s",
                       d.m_deviceAddress);
            }
            // Intentionally unchanged: legacy alignment - the BTMgr-based plugin connected inline on the event thread;
            // deinit() waits for in-flight callbacks, so this blocking BTRMGR connect is tolerated.
            if (accept && responded && !connectDevice(handleStr, dt ? dt : "UNKNOWN")) {
                LOGERR("Failed to connect device %s after auto-accept", d.m_deviceAddress);
            }
            return;
        }

        if (m_authCbs.onConnectionRequest)
            m_authCbs.onConnectionRequest(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                          d.m_vendorID, d.m_deviceAddress, profiles);
        break;
    }

    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST: {
        const auto& d = msg.m_externalDevice;
        std::string handleStr = std::to_string(d.m_deviceHandle);
        const char* dt = BTRMGR_GetDeviceTypeAsString(d.m_deviceType);
        const std::string profiles = joinProfiles(d.m_serviceInfo);
        cacheHandleToMac(handleStr, d.m_deviceAddress);
        if (m_authCbs.onPlaybackRequest)
            m_authCbs.onPlaybackRequest(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                        d.m_vendorID, d.m_deviceAddress, profiles);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_STARTED: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackChange)
            m_evtCbs.onPlaybackChange("started", static_cast<long long int>(m.m_deviceHandle),
                                       m.m_mediaPositionInfo.m_mediaDuration,
                                       m.m_mediaPositionInfo.m_mediaPosition);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_PAUSED: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackChange)
            m_evtCbs.onPlaybackChange("paused", static_cast<long long int>(m.m_deviceHandle),
                                       m.m_mediaPositionInfo.m_mediaDuration,
                                       m.m_mediaPositionInfo.m_mediaPosition);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_STOPPED: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackChange)
            m_evtCbs.onPlaybackChange("stopped", static_cast<long long int>(m.m_deviceHandle),
                                       m.m_mediaPositionInfo.m_mediaDuration,
                                       m.m_mediaPositionInfo.m_mediaPosition);
        break;
    }

    case BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackChange)
            m_evtCbs.onPlaybackChange("ended", static_cast<long long int>(m.m_deviceHandle), 0, 0);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_PLAYING:
    case BTRMGR_EVENT_MEDIA_TRACK_POSITION: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackProgress)
            m_evtCbs.onPlaybackProgress(static_cast<long long int>(m.m_deviceHandle),
                                        m.m_mediaPositionInfo.m_mediaDuration,
                                        m.m_mediaPositionInfo.m_mediaPosition);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_CHANGED: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onNewTrack)
            m_evtCbs.onNewTrack(static_cast<long long int>(m.m_deviceHandle),
                                m.m_mediaTrackInfo.pcAlbum,
                                m.m_mediaTrackInfo.pcGenre,
                                m.m_mediaTrackInfo.pcTitle,
                                m.m_mediaTrackInfo.pcArtist,
                                m.m_mediaTrackInfo.ui32Duration,
                                m.m_mediaTrackInfo.ui32TrackNumber,
                                m.m_mediaTrackInfo.ui32NumberOfTracks);
        break;
    }

    case BTRMGR_EVENT_DEVICE_MEDIA_STATUS: {
        const auto& m = msg.m_mediaInfo;
        const char* dt = BTRMGR_GetDeviceTypeAsString(m.m_deviceType);
        std::string command;
        switch (m.m_mediaDevStatus.m_enmediaCtrlCmd) {
            case BTRMGR_MEDIA_CTRL_VOLUMEUP:   command = "VOLUME_UP";    break;
            case BTRMGR_MEDIA_CTRL_VOLUMEDOWN: command = "VOLUME_DOWN";  break;
            case BTRMGR_MEDIA_CTRL_MUTE:       command = "AUDIO_MUTE";   break;
            case BTRMGR_MEDIA_CTRL_UNMUTE:     command = "AUDIO_UNMUTE"; break;
            default:                           command = "CMD_UNKNOWN"; break;
        }
        if (m_evtCbs.onDeviceMediaStatus)
            m_evtCbs.onDeviceMediaStatus(static_cast<long long int>(m.m_deviceHandle), m.m_name,
                                         dt ? dt : "UNKNOWN",
                                         m.m_mediaDevStatus.m_ui8mediaDevVolume,
                                         m.m_mediaDevStatus.m_ui8mediaDevMute != 0,
                                         command);
        break;
    }

    default:
        break;
    }
}


// ── Private helpers ────────────────────────────────────────────────────────────

// static
int BtMgrAdapterImpl::deviceOpTypeFromProfile(const std::string& p) {
    const bool hasAudio = Utils::String::contains(p, "LOUDSPEAKER") ||
                          Utils::String::contains(p, "HEADPHONES") ||
                          Utils::String::contains(p, "WEARABLE HEADSET") ||
                          Utils::String::contains(p, "HIFI AUDIO DEVICE");
    const bool hasHid = Utils::String::contains(p, "KEYBOARD") ||
                        Utils::String::contains(p, "MOUSE") ||
                        Utils::String::contains(p, "JOYSTICK");

    if (hasAudio && hasHid)
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_AND_HID);
    if (hasAudio)
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
    if (Utils::String::contains(p, "SMARTPHONE") || Utils::String::contains(p, "TABLET"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT);
    if (hasHid)
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_HID);
    // Intentionally unchanged: legacy alignment - substring matching is identical to the BTMgr plugin and
    // ScanFilterPolicy::modeFromProfile.
    if (Utils::String::contains(p, "LE TILE") || Utils::String::contains(p, "LE"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_LE);
    return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
}

void BtMgrAdapterImpl::cacheHandleToMac(const std::string& handleStr,
                                         const std::string& mac) const {  // mutable map members allow const
    std::lock_guard<std::mutex> lk(m_mapMutex);
    m_handleToMac[handleStr] = mac;
}

} // namespace Plugin
} // namespace WPEFramework
