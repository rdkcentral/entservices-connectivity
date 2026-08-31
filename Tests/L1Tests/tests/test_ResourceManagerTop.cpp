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

#include <gtest/gtest.h>

#include "ResourceManagerTop.h"

#include "DispatcherMock.h"
#include "FactoriesImplementation.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

#include <string>
#include "ResourceMonitorMock.h"

#include <Wraps.h>
#include <WrapsMocks.h>
#include <cstdio>


using namespace WPEFramework;
using ::WPEFramework::Core::hresult;
using ::testing::NiceMock;

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

class ResourceManagerTopTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::ResourceManagerTop> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    ServiceMock* mServiceMock = nullptr;
    PLUGINHOST_DISPATCHER* dispatcher = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    NiceMock<WrapsImplMock> wrapsImplMock;

    ResourceManagerTopTest()
        : plugin(Core::ProxyType<Plugin::ResourceManagerTop>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16))
    {
        PluginHost::IFactories::Assign(&factoriesImplementation);
        Core::IWorkerPool::Assign(&(*workerPool));

        Wraps::setImpl(&wrapsImplMock);

        workerPool->Run();
    }

    virtual ~ResourceManagerTopTest() override
    {
        TEST_LOG("ResourceManagerTopTest Destructor");
        Wraps::setImpl(nullptr);
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();
        PluginHost::IFactories::Assign(nullptr);
    }

    Core::hresult createResources()
    {
        mServiceMock = new NiceMock<ServiceMock>();
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
            plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        TEST_LOG("In createResources!");

        EXPECT_EQ(string(""), plugin->Initialize(mServiceMock));
        TEST_LOG("createResources - All done!");
        return Core::ERROR_NONE;
    }

    void releaseResources()
    {
        TEST_LOG("In releaseResources!");
        plugin->Deinitialize(mServiceMock);
        dispatcher->Deactivate();
        dispatcher->Release();
        delete mServiceMock;
        mServiceMock = nullptr;
        dispatcher = nullptr;
    }
};

// ── Information ──────────────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, GetInformation)
{
    EXPECT_EQ(string("{\"service\": \"org.rdk.ResourceManagerTop\"}"), plugin->Information());
}

// ── Method registration ───────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, RegisteredMethods)
{
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getApiVersionNumber")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSystemResourceInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("killProcess")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("killProcessViaResourceMonitor")));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

// ── getApiVersionNumber ───────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, GetApiVersionNumber)
{
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getApiVersionNumber"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"version\":1,\"success\":true}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

// ── getState ─────────────────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, GetState_WhenInitialized)
{
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"active\",\"success\":true}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, GetState_ServiceNotInitialized)
{
    // Activate dispatcher without calling Initialize so _service stays nullptr
    auto* serviceMock = new NiceMock<ServiceMock>();
    auto* disp = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
    disp->Activate(serviceMock);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getState"), _T("{}"), response));

    disp->Deactivate();
    disp->Release();
    delete serviceMock;
}

// ── getSystemResourceInfo ─────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, GetSystemResourceInfo_ServiceNotInitialized)
{
    auto* serviceMock = new NiceMock<ServiceMock>();
    auto* disp = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
    disp->Activate(serviceMock);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getSystemResourceInfo"), _T("{}"), response));

    disp->Deactivate();
    disp->Release();
    delete serviceMock;
}

TEST_F(ResourceManagerTopTest, GetSystemResourceInfo_Success)
{
    Core::hresult status = createResources();

    const std::string fakeTopOutput =
        "top - 12:00:00 up 1 day,  2 users,  load average: 0.10, 0.20, 0.30\n"
        "Tasks: 100 total,   1 running,  99 sleeping,   0 stopped,   0 zombie\n"
        "%Cpu(s):  5.0 us,  2.0 sy,  0.0 ni, 93.0 id\n"
        "MiB Mem :  4096.0 total,  1024.0 free\n";

    FILE* fakePipe = tmpfile();
    ASSERT_NE(fakePipe, nullptr);

    fputs(fakeTopOutput.c_str(), fakePipe);
    rewind(fakePipe);

        EXPECT_CALL(
        wrapsImplMock,
        popen(::testing::StrEq("top -n 1 -b | head -n 20"),
              ::testing::StrEq("r")))
        .WillOnce(::testing::Return(fakePipe));

    EXPECT_CALL(wrapsImplMock, pclose(fakePipe))
        .WillOnce(::testing::Invoke(
            [](FILE* pipe) {
                return fclose(pipe);
            }));

    
    // top is available on ubuntu CI; __wrap_popen (if active) must allow this to succeed
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSystemResourceInfo"), _T("{}"), response));

    JsonObject responseObj;
    responseObj.FromString(response);
    EXPECT_TRUE(responseObj["success"].Boolean());
    EXPECT_EQ(
        responseObj["resourceInfo"].String(),
        fakeTopOutput);
    
    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

// ── killProcess ───────────────────────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, KillProcess_MissingParameter)
{
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_BAD_REQUEST,
        handler.Invoke(connection, _T("killProcess"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"Missing required parameter: pid or processName\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcess_ByPid_NegativePid)
{
    // pid < 0 is caught before calling kill() - no system call made
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_GENERAL,
        handler.Invoke(connection, _T("killProcess"), _T("{\"pid\":-1}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"Failed to kill process\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcess_ByPid_NonExistentProcess)
{
    // PID 99999999 exceeds Linux pid_max - kill() will always fail with ESRCH/EINVAL
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_GENERAL,
        handler.Invoke(connection, _T("killProcess"), _T("{\"pid\":99999999}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"Failed to kill process\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcess_ByName_EmptyName)
{
    // empty processName is rejected before system("pkill ...") is called
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_GENERAL,
        handler.Invoke(connection, _T("killProcess"), _T("{\"processName\":\"\"}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"Failed to kill process\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

// ── killProcessViaResourceMonitor ─────────────────────────────────────────────

TEST_F(ResourceManagerTopTest, KillProcessViaResourceMonitor_MissingPid)
{
    Core::hresult status = createResources();

    EXPECT_EQ(Core::ERROR_BAD_REQUEST,
        handler.Invoke(connection, _T("killProcessViaResourceMonitor"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"Missing required parameter: pid\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcessViaResourceMonitor_NoResourceMonitor)
{
    Core::hresult status = createResources();

    // NiceMock returns nullptr by default from QueryInterfaceByCallsign
    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(nullptr));

    EXPECT_EQ(Core::ERROR_UNAVAILABLE,
        handler.Invoke(connection, _T("killProcessViaResourceMonitor"), _T("{\"pid\":1234}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"ResourceMonitor plugin not available\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcessViaResourceMonitor_Success)
{
    Core::hresult status = createResources();

    NiceMock<ResourceMonitorMock> rmMock;

    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.ResourceMonitor") {
                    return static_cast<void*>(&rmMock);
                }
                return nullptr;
            }));

    EXPECT_CALL(rmMock, KillProcess(1234, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke([](const int, bool& result) -> hresult {
            result = true;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_NONE,
        handler.Invoke(connection, _T("killProcessViaResourceMonitor"), _T("{\"pid\":1234}"), response));
    EXPECT_EQ(response, string("{\"success\":true,\"message\":\"Process killed via ResourceMonitor\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}

TEST_F(ResourceManagerTopTest, KillProcessViaResourceMonitor_KillFails)
{
    Core::hresult status = createResources();

    NiceMock<ResourceMonitorMock> rmMock;

    EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.ResourceMonitor") {
                    return static_cast<void*>(&rmMock);
                }
                return nullptr;
            }));

    EXPECT_CALL(rmMock, KillProcess(5678, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke([](const int, bool& result) -> hresult {
            result = false;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_GENERAL,
        handler.Invoke(connection, _T("killProcessViaResourceMonitor"), _T("{\"pid\":5678}"), response));
    EXPECT_EQ(response, string("{\"success\":false,\"message\":\"ResourceMonitor failed to kill process\"}"));

    if (Core::ERROR_NONE == status)
    {
        releaseResources();
    }
}
