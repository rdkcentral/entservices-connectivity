/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <gmock/gmock.h>
#include <interfaces/IResourceMonitor.h>

using ::WPEFramework::Core::hresult;
using ::WPEFramework::Exchange::IResourceMonitor;

// Flat MOCK_METHOD style matching entservices-testframework conventions (DeviceInfoMock.h).
// All pure virtuals from IResourceMonitor must appear here or the class cannot be instantiated.
class ResourceMonitorMock : public IResourceMonitor {
public:
    ResourceMonitorMock() = default;
    virtual ~ResourceMonitorMock() = default;

    // IResourceMonitor interface
    MOCK_METHOD(hresult, Register,               (IResourceMonitor::IProcessKilledNotification* notification),        (override));
    MOCK_METHOD(hresult, Unregister,             (const IResourceMonitor::IProcessKilledNotification* notification), (override));
    MOCK_METHOD(hresult, GetApiVersionNumber,    (int& version),                                                      (override));
    MOCK_METHOD(hresult, GetState,               (),                                                                  (override));
    MOCK_METHOD(hresult, GetSystemResourceInfo,  (string& topresult),                                                 (override));
    MOCK_METHOD(hresult, KillProcess,            (int PID, bool& result),                                             (override));

    // IUnknown interface
    MOCK_METHOD(uint32_t, AddRef,        (),                               (const, override));
    MOCK_METHOD(uint32_t, Release,       (),                               (const, override));
    MOCK_METHOD(void*,    QueryInterface, (const uint32_t interfaceNumber), (override));
};


// ─────────────────────────────────────────────────────────────────────────────
// ResourceMonitorMock
//
// Registry pattern (mirrors PowerManagerMock used in entservices-xcast):
//
//   Get()    → Exchange::IResourceMonitor*   pass to ServiceMock return value
//   Mock()   → ResourceMonitorMock&          call EXPECT_CALL on this
//   Delete() → void                          call in releaseResources()
//
// The static map is keyed by "TestSuite#TestName" so each test gets an
// independent instance and mocks can never bleed across tests.
// ─────────────────────────────────────────────────────────────────────────────
// class ResourceMonitorMock : public Exchange::IResourceMonitor {
// public:
//     // ── Mocked interface method ───────────────────────────────────────────────
//     MOCK_METHOD(uint32_t, KillProcess, (const int pid, bool& result), (override));

//     // ── WPEFramework COM wiring ───────────────────────────────────────────────
//     // Required so Thunder's QueryInterface resolves IResourceMonitor correctly.
//     BEGIN_INTERFACE_MAP(ResourceMonitorMock)
//     INTERFACE_ENTRY(Exchange::IResourceMonitor)
//     END_INTERFACE_MAP

//     // ── Registry API ─────────────────────────────────────────────────────────

//     // Creates (or retrieves) the mock for the currently-running test.
//     static Exchange::IResourceMonitor* Get()
//     {
//         const std::string id = testId();
//         ASSERT(!id.empty());

//         auto& map = instances();
//         auto  it  = map.find(id);
//         if (it == map.end()) {
//             map.insert({id, Core::ProxyType<ResourceMonitorMock>::Create()});
//             it = map.find(id);
//         }
//         return &(*(it->second));
//     }

//     // Returns the typed reference so EXPECT_CALL(ResourceMonitorMock::Mock(), ...) compiles.
//     static ResourceMonitorMock& Mock()
//     {
//         return *static_cast<ResourceMonitorMock*>(Get());
//     }

//     // Removes this test's entry; ProxyType destructor cleans up the object.
//     static void Delete()
//     {
//         const std::string id = testId();
//         if (!id.empty()) {
//             instances().erase(id);
//         }
//     }

// private:
//     // Unique key per test: "SuiteName#TestName"
//     static std::string testId()
//     {
//         const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
//         if (!info) return {};
//         return std::string(info->test_suite_name()) + "#" + info->name();
//     }

//     // Singleton map; each test's mock lives here until Delete() is called.
//     static std::map<std::string, Core::ProxyType<Exchange::IResourceMonitor>>& instances()
//     {
//         static std::map<std::string, Core::ProxyType<Exchange::IResourceMonitor>> map;
//         return map;
//     }
// };
