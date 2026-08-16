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

#include <cstdlib>
#include <cstring>

#include "btmgr.h"
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

} // namespace

// ── Static instance pointer for IARM callback ────────────────────────────────

BtMgrAdapterImpl* BtMgrAdapterImpl::s_instance = nullptr;

// ── Lifecycle ────────────────────────────────────────────────────────────────

std::string BtMgrAdapterImpl::init(PluginHost::IShell* /* service */,
                                    BtEventCallbacks eventCallbacks,
                                    BtAuthCallbacks  authCallbacks) {
    m_evtCbs  = std::move(eventCallbacks);
    m_authCbs = std::move(authCallbacks);
    s_instance = this;

    BTRMGR_Result_t rc = BTRMGR_RegisterForCallbacks(kIarmClientName);
    if (rc != BTRMGR_RESULT_SUCCESS) {
        s_instance = nullptr;
        return std::string("BTRMGR_RegisterForCallbacks failed: ") + std::to_string(rc);
    }

    BTRMGR_RegisterEventCallback(
        [](BTRMGR_EventMessage_t eventMsg) -> BTRMGR_Result_t {
            if (s_instance) {
                 s_instance->onEvent(&eventMsg, sizeof(eventMsg));
                 return BTRMGR_RESULT_SUCCESS;
             }
             return BTRMGR_RESULT_INIT_FAILED;
        });

    return "";
}

void BtMgrAdapterImpl::deinit() {
    s_instance = nullptr;
    BTRMGR_UnRegisterFromCallbacks(kIarmClientName);
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
    BTRMGR_DeviceOperationType_t opType =
        static_cast<BTRMGR_DeviceOperationType_t>(deviceOpTypeFromProfile(profile));
    return BTRMGR_StartDeviceDiscovery(0, opType) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::stopScan() {
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
    for (int i = 0; i < list.m_numOfDevices; ++i) {
        const auto& d = list.m_deviceProperty[i];
        auto info = deviceInfoFromBtmgr(d, d.m_isPairedDevice != 0);
        cacheHandleToMac(info.handleStr, info.mac);
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<IBtAdapter::BtDeviceInfo> BtMgrAdapterImpl::getPairedDevices() const {
    std::vector<BtDeviceInfo> result;
    BTRMGR_PairedDevicesList_t list;
    memset(&list, 0, sizeof(list));
    if (BTRMGR_GetPairedDevices(0, &list) != BTRMGR_RESULT_SUCCESS) return result;
    for (int i = 0; i < list.m_numOfDevices; ++i) {
        auto info = deviceInfoFromBtmgr(list.m_deviceProperty[i], true);
        cacheHandleToMac(info.handleStr, info.mac);
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<IBtAdapter::BtDeviceInfo> BtMgrAdapterImpl::getConnectedDevices() const {
    std::vector<BtDeviceInfo> result;
    BTRMGR_ConnectedDevicesList_t list;
    memset(&list, 0, sizeof(list));
    if (BTRMGR_GetConnectedDevices(0, &list) != BTRMGR_RESULT_SUCCESS) return result;
    for (int i = 0; i < list.m_numOfDevices; ++i) {
        auto info = deviceInfoFromBtmgr(list.m_deviceProperty[i], true);
        cacheHandleToMac(info.handleStr, info.mac);
        result.push_back(std::move(info));
    }
    return result;
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtMgrAdapterImpl::pairDevice(const std::string& handleStr) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    return BTRMGR_PairDevice(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::unpairDevice(const std::string& handleStr) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    return BTRMGR_UnpairDevice(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::connectDevice(const std::string& handleStr, const std::string& deviceType) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    if (isAudioOutputDeviceType(deviceType))
        return BTRMGR_StartAudioStreamingOut(0, h, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) == BTRMGR_RESULT_SUCCESS;
    if (isAudioInputDeviceType(deviceType))
        return BTRMGR_StartAudioStreamingIn(0, h, BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) == BTRMGR_RESULT_SUCCESS;
    return BTRMGR_ConnectToDevice(0, h, BTRMGR_DEVICE_OP_TYPE_UNKNOWN) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& deviceType) {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    if (isAudioOutputDeviceType(deviceType))
        return BTRMGR_StopAudioStreamingOut(0, h) == BTRMGR_RESULT_SUCCESS;
    if (isAudioInputDeviceType(deviceType))
        return BTRMGR_StopAudioStreamingIn(0, h) == BTRMGR_RESULT_SUCCESS;
    return BTRMGR_DisconnectFromDevice(0, h) == BTRMGR_RESULT_SUCCESS;
}

bool BtMgrAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                             BtDeviceProperties& props) const {
    BTRMgrDeviceHandle h = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    BTRMGR_DevicesProperty_t p;
    memset(&p, 0, sizeof(p));
    if (BTRMGR_GetDeviceProperties(0, h, &p) != BTRMGR_RESULT_SUCCESS) return false;

    props.handleStr    = handleStr;
    props.mac          = p.m_deviceAddress;
    props.name         = p.m_name;
    const char* dt     = BTRMGR_GetDeviceTypeAsString(p.m_deviceType);
    props.deviceType   = dt ? dt : "UNKNOWN";
    props.rssi         = static_cast<int16_t>(p.m_rssi);
    props.signalLevel  = static_cast<int16_t>(p.m_signalLevel);
    props.batteryLevel = p.m_batteryLevel;
    props.vendorId     = static_cast<uint16_t>(p.m_vendorID);
    props.modalias     = p.m_modalias;

    for (int i = 0; i < p.m_serviceInfo.m_numOfService; ++i) {
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

bool BtMgrAdapterImpl::respondToEvent(const std::string& mac, bool accepted) {
    int eventType = 0;
    {
        std::lock_guard<std::mutex> lk(m_pendingMutex);
        if (m_pendingMac != mac || m_pendingEventType == 0) return false;
        eventType = m_pendingEventType;
        m_pendingMac.clear();
        m_pendingEventType = 0;
    }

    return respondToEvent(mac, eventType, accepted);
}

bool BtMgrAdapterImpl::respondToEvent(const std::string& mac,
                                      int eventType,
                                      bool accepted) {

    BTRMGR_EventResponse_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    // Recover handle from MAC.
    std::string handleStr;
    {
        std::lock_guard<std::mutex> lk2(m_mapMutex);
        auto it = m_macToHandle.find(mac);
        if (it != m_macToHandle.end()) handleStr = it->second;
    }
    if (handleStr.empty()) return false;

    rsp.m_deviceHandle = static_cast<BTRMgrDeviceHandle>(std::stoll(handleStr));
    rsp.m_eventType    = static_cast<BTRMGR_Events_t>(eventType);
    rsp.m_eventResp    = accepted ? 1 : 0;

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
    if (cmd == "MUTE")          return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_MUTE)      == BTRMGR_RESULT_SUCCESS;
    if (cmd == "UNMUTE")        return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_UNMUTE)    == BTRMGR_RESULT_SUCCESS;
    if (cmd == "VOLUME_UP")     return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_VOLUMEUP)  == BTRMGR_RESULT_SUCCESS;
    if (cmd == "VOLUME_DOWN")   return BTRMGR_MediaControl(0, h, BTRMGR_MEDIA_CTRL_VOLUMEDOWN) == BTRMGR_RESULT_SUCCESS;
    if (cmd == "AUDIO_IN")      return BTRMGR_StartAudioStreamingIn(0, h,
                                           BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) == BTRMGR_RESULT_SUCCESS;
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
                                        d.m_isLastConnectedDevice != 0, "NEW");
        break;
    }

    case BTRMGR_EVENT_DEVICE_FOUND: {
        const auto& d = msg.m_discoveredDevice;
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
        const auto& d = msg.m_discoveredDevice;
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
        bool hasAC = false; bool ac = false;
        if (m_evtCbs.getAutoConnect) m_evtCbs.getAutoConnect(handleStr, ac), hasAC = true;
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
        bool hasAC = false; bool ac = false;
        if (m_evtCbs.getAutoConnect) m_evtCbs.getAutoConnect(handleStr, ac), hasAC = true;
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

    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED:
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED: {
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
        std::string profiles;
        for (int i = 0; i < d.m_serviceInfo.m_numOfService; ++i) {
            if (!profiles.empty()) profiles += ";";
            profiles += d.m_serviceInfo.m_profileInfo[i].m_profile;
        }
        cacheHandleToMac(handleStr, d.m_deviceAddress);
        {
            std::lock_guard<std::mutex> lk(m_pendingMutex);
            m_pendingMac       = d.m_deviceAddress;
            m_pendingEventType = static_cast<int>(BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST);
        }
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
        std::string profiles;
        for (int i = 0; i < d.m_serviceInfo.m_numOfService; ++i) {
            if (!profiles.empty()) profiles += ";";
            profiles += d.m_serviceInfo.m_profileInfo[i].m_profile;
        }
        cacheHandleToMac(handleStr, d.m_deviceAddress);

        // Check if paired device should auto-accept (mirrors AuthBridge policy).
        bool autoAccept = false;
        if (m_authCbs.isPaired && m_authCbs.isPaired(handleStr)) {
            autoAccept = true;
        }
        if (autoAccept) {
            if (!respondToEvent(
                    d.m_deviceAddress,
                    static_cast<int>(BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST),
                    true)) {
                LOGERR("Failed to auto-accept connection request for %s",
                       d.m_deviceAddress);
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lk(m_pendingMutex);
            m_pendingMac       = d.m_deviceAddress;
            m_pendingEventType = static_cast<int>(BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);
        }
        if (m_authCbs.onConnectionRequest)
            m_authCbs.onConnectionRequest(handleStr, d.m_name, dt ? dt : "UNKNOWN",
                                          d.m_vendorID, d.m_deviceAddress, profiles);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_STARTED:
    case BTRMGR_EVENT_MEDIA_TRACK_PLAYING: {
        const auto& m = msg.m_mediaInfo;
        if (m_evtCbs.onPlaybackChange)
            m_evtCbs.onPlaybackChange("started", static_cast<long long int>(m.m_deviceHandle),
                                       m.m_mediaPositionInfo.m_mediaDuration,
                                       m.m_mediaPositionInfo.m_mediaPosition);
        break;
    }

    case BTRMGR_EVENT_MEDIA_TRACK_PAUSED:
    case BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED: {
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

    default:
        break;
    }
}

// ── Private helpers ────────────────────────────────────────────────────────────

// static
std::string BtMgrAdapterImpl::deriveHandle(const std::string& mac) {
    if (mac.size() < 17) return "0";
    char h[13] = {};
    h[0]=mac[0]; h[1]=mac[1]; h[2]=mac[3];  h[3]=mac[4];
    h[4]=mac[6]; h[5]=mac[7]; h[6]=mac[9];  h[7]=mac[10];
    h[8]=mac[12];h[9]=mac[13];h[10]=mac[15];h[11]=mac[16];
    return std::to_string(static_cast<uint64_t>(strtoll(h, nullptr, 16)));
}

// static
int BtMgrAdapterImpl::deviceOpTypeFromProfile(const std::string& p) {
    if (Utils::String::contains(p, "LOUDSPEAKER") ||
        Utils::String::contains(p, "HEADPHONES") ||
        Utils::String::contains(p, "WEARABLE HEADSET") ||
        Utils::String::contains(p, "HIFI AUDIO DEVICE"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
    if (Utils::String::contains(p, "SMARTPHONE") || Utils::String::contains(p, "TABLET"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT);
    if (Utils::String::contains(p, "KEYBOARD") ||
        Utils::String::contains(p, "MOUSE") ||
        Utils::String::contains(p, "JOYSTICK"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_HID);
    if (Utils::String::contains(p, "LE TILE") || Utils::String::contains(p, "LE"))
        return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_LE);
    return static_cast<int>(BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
}

// static
bool BtMgrAdapterImpl::isAudioOutputDeviceType(const std::string& t) {
    return t == "LOUDSPEAKER" || t == "HEADPHONES"
        || t == "WEARABLE HEADSET" || t == "HIFI AUDIO DEVICE" || t == "HANDSFREE";
}

// static
bool BtMgrAdapterImpl::isAudioInputDeviceType(const std::string& t) {
    return t == "SMARTPHONE" || t == "TABLET";
}

void BtMgrAdapterImpl::cacheHandleToMac(const std::string& handleStr,
                                         const std::string& mac) const {  // mutable map members allow const
    std::lock_guard<std::mutex> lk(m_mapMutex);
    m_handleToMac[handleStr] = mac;
    m_macToHandle[mac]       = handleStr;
}

// static
int BtMgrAdapterImpl::staticEventCallback(const char* /*owner*/, int /*eventId*/,
                                            void* data, size_t len) {
    if (s_instance) s_instance->onEvent(data, len);
    return 0;
}

} // namespace Plugin
} // namespace WPEFramework
