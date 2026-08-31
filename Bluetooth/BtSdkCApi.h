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

#pragma once

// DRAFT — not yet agreed with the bluetooth-sdk team. See
// docs/bluetooth-sdk-c-facade-proposal.md for the rationale and the request.
//
// Stable C ABI facade for librdk_bluetooth.so. Pure C: no C++ classes, no
// sdbus-c++ types, no vtables cross this boundary, so this header has no
// dependency on the real SDK's C++ headers or on sdbus-c++. It is safe to
// vendor permanently and compile unconditionally in the common plugin build,
// on every product, whether or not the real library is present. Only a single
// symbol (BtSdkCApi_GetTable) is resolved with dlsym(); everything else is
// reached through the returned function-pointer table.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTSDK_CAPI_ABI_VERSION 1u

typedef struct BtSdkManager_* BtSdkManagerHandle;
typedef struct BtSdkAdapter_* BtSdkAdapterHandle;

typedef enum {
    BTSDK_OK             = 0,
    BTSDK_ERROR          = 1,
    BTSDK_NOT_SUPPORTED  = 2,
} BtSdkStatus;

typedef enum {
    BTSDK_DEVICE_STATE_DISCOVERED = 0,
    BTSDK_DEVICE_STATE_PAIRED     = 1,
    BTSDK_DEVICE_STATE_CONNECTED  = 2,
} BtSdkDeviceState;

typedef enum {
    BTSDK_EVENT_DISCOVERY_STARTED  = 0,
    BTSDK_EVENT_DISCOVERY_STOPPED  = 1,
    BTSDK_EVENT_POWERED_ON         = 2,
    BTSDK_EVENT_POWERED_OFF        = 3,
    BTSDK_EVENT_DEVICE_DISCOVERED  = 4,
    BTSDK_EVENT_DEVICE_DISAPPEARED = 5,
} BtSdkAdapterEvent;

typedef enum {
    BTSDK_AUTH_PAIRING_REQUEST    = 0,
    BTSDK_AUTH_CONNECTION_REQUEST = 1,
} BtSdkAuthorisationType;

// All pointer/array fields are borrowed for the duration of the callback or
// call; callers must copy anything they need to keep.
typedef struct {
    const char*        mac;
    const char*        name;
    uint32_t           classOfDevice;
    uint16_t           appearance;
    const char* const* uuids;
    int                uuidCount;
    int                paired;
    int                connected;
} BtSdkDeviceInfo;

typedef void (*BtSdkAdapterEventCb)(void* userdata, BtSdkAdapterEvent event, const BtSdkDeviceInfo* device);

// Returns non-zero to accept the request, zero to reject.
typedef int (*BtSdkAuthRequestCb)(void* userdata, BtSdkAuthorisationType type,
                                  const char* mac, const char* deviceType);

typedef struct {
    // Bump on any incompatible change to this table's layout. Callers must
    // check this before calling any function pointer below.
    uint32_t abiVersion;

    BtSdkManagerHandle (*ManagerCreate)(BtSdkAuthRequestCb authCb, void* authUserdata);
    void               (*ManagerDestroy)(BtSdkManagerHandle mgr);
    BtSdkStatus        (*ManagerGetDefaultAdapter)(BtSdkManagerHandle mgr, BtSdkAdapterHandle* outAdapter);

    BtSdkStatus (*AdapterGetPowered)(BtSdkAdapterHandle adapter, int* outPowered);
    BtSdkStatus (*AdapterSetPowered)(BtSdkAdapterHandle adapter, int powered);
    BtSdkStatus (*AdapterStartScan)(BtSdkAdapterHandle adapter, const char* profile);
    BtSdkStatus (*AdapterStopScan)(BtSdkAdapterHandle adapter);
    BtSdkStatus (*AdapterRegisterEvents)(BtSdkAdapterHandle adapter, BtSdkAdapterEventCb cb, void* userdata);
    BtSdkStatus (*AdapterUnregisterEvents)(BtSdkAdapterHandle adapter);

    // Caller-allocated array of maxCount entries; outCount is set to however
    // many were written (never more than maxCount).
    BtSdkStatus (*AdapterGetDevices)(BtSdkAdapterHandle adapter, BtSdkDeviceState state,
                                     BtSdkDeviceInfo* outDevices, int maxCount, int* outCount);

    BtSdkStatus (*DevicePair)(BtSdkAdapterHandle adapter, const char* mac);
    BtSdkStatus (*DeviceUnpair)(BtSdkAdapterHandle adapter, const char* mac);
    BtSdkStatus (*DeviceConnect)(BtSdkAdapterHandle adapter, const char* mac, const char* deviceType);
    BtSdkStatus (*DeviceDisconnect)(BtSdkAdapterHandle adapter, const char* mac, const char* deviceType);
    BtSdkStatus (*DeviceGetProperties)(BtSdkAdapterHandle adapter, const char* mac, BtSdkDeviceInfo* outProps);
} BtSdkCApiTable;

// The one symbol resolved with dlsym(); everything else is reached through
// the returned table. Implemented by librdk_bluetooth.so.
const BtSdkCApiTable* BtSdkCApi_GetTable(void);

#ifdef __cplusplus
}
#endif
