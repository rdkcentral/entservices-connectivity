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

/*
 * L2 test suite for the ResourceManagerTop plugin.
 *
 * ResourceManagerTop is JSON-RPC only -- it does not expose any COM-RPC interface
 * of its own. All method calls therefore go through InvokeServiceMethod() (a real
 * JSON-RPC round trip via the running Thunder instance), never the C++ plugin
 * object directly.
 *
 * killProcessViaResourceMonitor() is the one method that depends on another
 * plugin: at request time it asks the active IShell for "org.rdk.ResourceMonitor"
 * as Exchange::IResourceMonitor and calls KillProcess(pid, result) on it. This
 * suite activates the REAL org.rdk.ResourceMonitor plugin (checked out and built
 * from its own repository -- see .github/workflows/L2-tests.yml) rather than a
 * test-only stub, so no ResourceMonitor implementation is written here.
 *
 * getSystemResourceInfo() still runs "top" via popen(); popen() is intercepted
 * process-wide via the linker "-Wl,-wrap,popen" flag (same mechanism L1 uses),
 * so it is mocked here through p_wrapsImplMock for deterministic output instead
 * of depending on the CI host's real `top` command.
 */

#include "L2Tests.h"
#include "L2TestsMock.h"
#include <cstdio>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define JSON_TIMEOUT (5000)

#define RMTOP_ACTIVATE_CALLSIGN   "org.rdk.ResourceManagerTop"
#define RMTOP_INVOKE_CALLSIGN     _T("org.rdk.ResourceManagerTop.1")
#define RESOURCEMONITOR_CALLSIGN "org.rdk.ResourceMonitor"

/* A PID this large always exceeds Linux's pid_max, so kill()/ResourceMonitor's
 * equivalent call are guaranteed to fail without affecting any real process. */
#define NON_EXISTENT_PID 999999999

using namespace WPEFramework;

namespace {
bool g_resourceMonitorActive = false;
bool g_resourceManagerTopActive = false;

/* L2TestMocks::TestBody() is pure virtual (testing::Test), so a bare
 * L2TestMocks-derived object can't be instantiated outside a TEST_F macro.
 * This throwaway harness (mirrors Bluetooth_L2Test's BluetoothSuiteHarness)
 * only exists to expose Activate/Deactivate for suite setup/teardown. */
class ResourceManagerTopSuiteHarness : public L2TestMocks {
public:
    ResourceManagerTopSuiteHarness() : L2TestMocks() {}
    void TestBody() override {}

    uint32_t Activate(const char* callsign) { return ActivateService(callsign); }
    uint32_t Deactivate(const char* callsign) { return DeactivateService(callsign); }
};
} // namespace

class ResourceManagerTop_L2Test : public L2TestMocks {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
};

void ResourceManagerTop_L2Test::SetUpTestSuite()
{
    ResourceManagerTopSuiteHarness harness;

    uint32_t status = harness.Activate(RESOURCEMONITOR_CALLSIGN);
    g_resourceMonitorActive = (status == Core::ERROR_NONE);
    ASSERT_EQ(Core::ERROR_NONE, status);

    status = harness.Activate(RMTOP_ACTIVATE_CALLSIGN);
    g_resourceManagerTopActive = (status == Core::ERROR_NONE);
    ASSERT_EQ(Core::ERROR_NONE, status);
}

void ResourceManagerTop_L2Test::TearDownTestSuite()
{
    ResourceManagerTopSuiteHarness harness;

    if (g_resourceManagerTopActive) {
        EXPECT_EQ(Core::ERROR_NONE, harness.Deactivate(RMTOP_ACTIVATE_CALLSIGN));
        g_resourceManagerTopActive = false;
    }
    if (g_resourceMonitorActive) {
        EXPECT_EQ(Core::ERROR_NONE, harness.Deactivate(RESOURCEMONITOR_CALLSIGN));
        g_resourceMonitorActive = false;
    }
}

/* =========================================================================
 * TC-01: getApiVersionNumber
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, GetApiVersionNumber)
{
    JsonObject result;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "getApiVersionNumber", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_GT(result["version"].Number(), 0u);
}

/* =========================================================================
 * TC-02: getState
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, GetState)
{
    JsonObject result;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "getState", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_STREQ("active", result["state"].String().c_str());
}

/* =========================================================================
 * TC-03: getSystemResourceInfo
 * popen() is mocked so the assertion does not depend on the CI host's `top`.
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, GetSystemResourceInfo)
{
    const std::string fakeTopOutput =
        "top - 12:00:00 up 1 day,  2 users,  load average: 0.10, 0.20, 0.30\n"
        "Tasks: 100 total,   1 running,  99 sleeping,   0 stopped,   0 zombie\n"
        "%Cpu(s):  5.0 us,  2.0 sy,  0.0 ni, 93.0 id\n";

    FILE* fakePipe = tmpfile();
    ASSERT_NE(fakePipe, nullptr);
    fputs(fakeTopOutput.c_str(), fakePipe);
    rewind(fakePipe);

    EXPECT_CALL(*p_wrapsImplMock,
        popen(::testing::StrEq("top -n 1 -b | head -n 20"), ::testing::StrEq("r")))
        .WillOnce(::testing::Return(fakePipe));

    JsonObject result;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "getSystemResourceInfo", result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ(fakeTopOutput, result["resourceInfo"].String());
}

/* =========================================================================
 * TC-04: killProcess -- missing parameter
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillProcessMissingParameter)
{
    JsonObject params, result;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcess", params, result);
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-05: killProcess by PID -- negative PID rejected before kill() is called
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillProcessNegativePidRejected)
{
    JsonObject params, result;
    params["pid"] = -1;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcess", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-06: killProcess by PID -- PID exceeds pid_max, kill() fails, no real
 * process is affected.
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillProcessNonExistentPid)
{
    JsonObject params, result;
    params["pid"] = NON_EXISTENT_PID;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcess", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-07: killProcess by name -- empty name rejected before system("pkill ...")
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillProcessByNameEmptyNameRejected)
{
    JsonObject params, result;
    params["processName"] = "";
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcess", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-08: killProcessViaResourceMonitor -- missing parameter
 * Validated by ResourceManagerTop itself; never reaches ResourceMonitor.
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillViaResourceMonitorMissingPid)
{
    JsonObject params, result;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcessViaResourceMonitor", params, result);
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-09: killProcessViaResourceMonitor -- PID exceeds pid_max
 * Assumes the real ResourceMonitor.KillProcess() reports failure for a PID
 * that cannot exist on Linux, mirroring ResourceManagerTop's own killProcess
 * contract. Adjust this expectation if ResourceMonitor's behavior differs.
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillViaResourceMonitorNonExistentPid)
{
    JsonObject params, result;
    params["pid"] = NON_EXISTENT_PID;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcessViaResourceMonitor", params, result);
    EXPECT_EQ(Core::ERROR_GENERAL, status);
    EXPECT_FALSE(result["success"].Boolean());
}

/* =========================================================================
 * TC-10: killProcessViaResourceMonitor -- ResourceMonitor plugin unavailable
 * Deactivates ResourceMonitor for the duration of this test only; a scope
 * guard reactivates it afterwards regardless of assertion outcome or test
 * execution order, so the rest of the suite is unaffected.
 * ====================================================================== */
TEST_F(ResourceManagerTop_L2Test, KillViaResourceMonitorUnavailable)
{
    /* The lambda runs with the enclosing TestBody()'s access, so it may call
     * the fixture's protected ActivateService(); a nested struct could not. */
    struct ScopeExit {
        std::function<void()> fn;
        ~ScopeExit() { fn(); }
    } reactivate{ [this]() {
        EXPECT_EQ(Core::ERROR_NONE, ActivateService(RESOURCEMONITOR_CALLSIGN));
    } };

    ASSERT_EQ(Core::ERROR_NONE, DeactivateService(RESOURCEMONITOR_CALLSIGN));

    JsonObject params, result;
    params["pid"] = 1234;
    uint32_t status = InvokeServiceMethod(RMTOP_INVOKE_CALLSIGN, "killProcessViaResourceMonitor", params, result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, status);
    EXPECT_FALSE(result["success"].Boolean());
}
