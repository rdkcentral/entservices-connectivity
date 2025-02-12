# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.8)
project(RdkServicesL1Test)

set(CMAKE_CXX_STANDARD 11)

find_package(${NAMESPACE}Plugins REQUIRED)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/e39786088138f2749d64e9e90e0f9902daa77c40.zip
)
FetchContent_MakeAvailable(googletest)

set (TEST_SRC
    tests/test_UtilsFile.cpp
    tests/test_FirmwareVersion.cpp
    tests/test_JSON.cpp
    tests/test_DeviceAudioCapabilities.cpp
)

set (TEST_LIB
    gmock_main
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

set (TEST_INC ../../helpers)

#########################################################################################
# add_plugin_test_ex: Macro to add plugin tests, it will append to TEST_SRC, TEST_INC,
#                     and TEST_LIB. Args are positional.
#                     arg1: test file list as **string**
#                     arg2: include dir list as **string**
#                     arg3: plugin libs to link list as **string**
#
# sample invocation : 
#    add_plugin_test_ex(PLUGIN_NAME
#                       "test/file1.cpp;test/file2.cpp"
#                       "../../inc_dir1;../../inc_dir2"
#                       "${NAMESPACE}PluginName;${NAMESPACE}PluginNameImplementation")
# ----------------------------------------- OR ------------------------------------------
# list(APPEND PLUGIN_NAME_SRC test/file1.cpp)
# list(APPEND PLUGIN_NAME_SRC test/file2.cpp)
#
# list(APPEND PLUGIN_NAME_INC ../../inc_dir1)
# list(APPEND PLUGIN_NAME_INC ../../inc_dir2)
#
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginName)
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginNameImplementation)
#
#    add_plugin_test_ex(PLUGIN_NAME
#                       "${PLUGIN_NAME_SRC}"
#                       "${PLUGIN_NAME_INC}"
#                       "${PLUGIN_NAME_LIB}")
#
# NOTE: Alternatively test can choose to update `TEST_SRC`, `TEST_INC` & `TEST_LIB`
#       directly (like in the case of Miracast)
#########################################################################################
macro(add_plugin_test_ex plugin_opt plugin_test_sources_str plugin_includes_str plugin_libs_str)
    # Check if the plugin option is enabled
    if(${plugin_opt})
        message(STATUS "${plugin_opt}=ON")

        string(REPLACE ";" ";" srclist "${plugin_test_sources_str}")
        string(REPLACE ";" ";" inclist "${plugin_includes_str}")
        string(REPLACE ";" ";" liblist "${plugin_libs_str}")

        foreach(item IN LISTS srclist)
            # Add each test source file
            list(APPEND TEST_SRC ${item})
        endforeach()

        foreach(item IN LISTS inclist)
            # Add each include directory
            list(APPEND TEST_INC ${item})
        endforeach()

        foreach(item IN LISTS liblist)
            # Add each libraries to link
            list(APPEND TEST_LIB ${item})
        endforeach()
    else()
        message(STATUS "${plugin_opt}=OFF")
    endif()
endmacro()

# helper to add plugin test
macro(add_plugin_test plugin_name test_files)
    # Convert plugin name to uppercase for the option variable
    string(TOUPPER "${plugin_name}" plugin_option)
    set(plugin_opt "PLUGIN_${plugin_option}")

    add_plugin_test_ex(${plugin_opt} "${test_files}" "../../${plugin_name}" "${NAMESPACE}${plugin_name}")
endmacro()

add_plugin_test(AVInput tests/test_AVInput.cpp)
add_plugin_test(CompositeInput tests/test_CompositeInput.cpp)
add_plugin_test(DeviceDiagnostics tests/test_DeviceDiagnostics.cpp)

set (DEVICEINFO_SRC tests/test_DeviceInfo.cpp
                    tests/test_DeviceInfoJsonRpc.cpp
                    tests/test_DeviceInfoWeb.cpp
                    tests/test_DeviceVideoCapabilities.cpp)

# PLUGIN_DEVICEINFO
add_plugin_test(DeviceInfo "${DEVICEINFO_SRC}")

# PLUGIN_FRAMERATE
add_plugin_test(FrameRate tests/test_FrameRate.cpp)

# PLUGIN_FRONTPANEL
add_plugin_test(FrontPanel tests/test_FrontPanel.cpp)

# PLUGIN_HDCPPROFILE
add_plugin_test(HdcpProfile tests/test_HdcpProfile.cpp)

# PLUGIN_HDMICEC2
add_plugin_test_ex(PLUGIN_HDMICEC2 tests/test_HdmiCec2.cpp "../../HdmiCec_2" "${NAMESPACE}HdmiCec_2")

# PLUGIN_HDMICECSINK
add_plugin_test(HdmiCecSink tests/test_HdmiCecSink.cpp)

# PLUGIN_HDMICECSOURCE
add_plugin_test(HdmiCecSource tests/test_HdmiCecSource.cpp)

# PLUGIN_HDMIINPUT
add_plugin_test(HdmiInput tests/test_HdmiInput.cpp)

# PLUGIN_MAINTENANCEMANAGER
add_plugin_test(MaintenanceManager tests/test_MaintenanceManager.cpp)

# PLUGIN_MIRACAST
if(PLUGIN_MIRACAST)
    # test src file
    list(APPEND TEST_SRC tests/test_MiracastPlayer.cpp)
    list(APPEND TEST_SRC tests/test_MiracastService.cpp)
    # libs to link
    list(APPEND TEST_LIB ${NAMESPACE}MiracastPlayer)
    list(APPEND TEST_LIB ${NAMESPACE}MiracastService)
    # includes
    list(APPEND TEST_INC ../../Miracast/MiracastPlayer)
    list(APPEND TEST_INC ../../Miracast/MiracastPlayer/RTSP)
    list(APPEND TEST_INC ../../Miracast/MiracastService)
    list(APPEND TEST_INC ../../Miracast/MiracastService/P2P)
    list(APPEND TEST_INC ../../Miracast/common)
endif()

# PLUGIN_MOTION_DETECTION
add_plugin_test_ex(PLUGIN_MOTION_DETECTION tests/test_MotionDetection.cpp "../../MotionDetection" "${NAMESPACE}MotionDetection")

# PLUGIN_NETWORK
add_plugin_test(Network tests/test_Network.cpp)

# PLUGIN_OCICONTAINER
add_plugin_test(OCIContainer tests/test_OCIContainer.cpp)

# PLUGIN_PACKAGER
add_plugin_test(Packager tests/test_Packager.cpp)

# PLUGIN_RDKSHELL
add_plugin_test(RDKShell tests/test_RDKShell.cpp)

# PLUGIN_SCREENCAPTURE
add_plugin_test(ScreenCapture tests/test_ScreenCapture.cpp)

# PLUGIN_SYSTEMAUDIOPLAYER
add_plugin_test(SystemAudioPlayer tests/test_SystemAudioPlayer.cpp)

# PLUGIN_SYSTEMSERVICES
add_plugin_test(SystemServices tests/test_SystemServices.cpp)

# PLUGIN_TELEMETRY
add_plugin_test(Telemetry tests/test_Telemetry.cpp)

# PLUGIN_TEXTTOSPEECH
add_plugin_test(TextToSpeech tests/test_TextToSpeech.cpp)

# PLUGIN_TRACECONTROL
set (TRACECONTROL_SRC tests/test_TraceControlJsonRpc.cpp
                      tests/test_TraceControlWeb.cpp)
add_plugin_test(TraceControl "${TRACECONTROL_SRC}")

# PLUGIN_USBDEVICE
set (USBDEVICE_LIBS ${NAMESPACE}USBDevice ${NAMESPACE}USBDeviceImplementation)
add_plugin_test_ex(PLUGIN_USBDEVICE tests/test_USBDevice.cpp "../../USBDevice" "${USBDEVICE_LIBS}")

# PLUGIN_USBACCESS
add_plugin_test(UsbAccess tests/test_UsbAccess.cpp)

# PLUGIN_USERPREFERENCES
add_plugin_test(UserPreferences tests/test_UserPreferences.cpp)

# PLUGIN_WAREHOUSE
add_plugin_test(Warehouse tests/test_Warehouse.cpp)

# PLUGIN_WIFIMANAGER
add_plugin_test(WifiManager tests/test_WifiManager.cpp)

# PLUGIN_XCAST
add_plugin_test(XCast tests/test_XCast.cpp)

# PLUGIN_APPMANAGER
add_plugin_test(AppManager tests/test_AppManager.cpp)

# PLUGIN_FIRMWAREUPDATE
set (FIRMWAREUPDATE_LIBS ${NAMESPACE}FirmwareUpdate ${NAMESPACE}FirmwareUpdateImplementation)
add_plugin_test_ex(PLUGIN_FIRMWAREUPDATE tests/test_FirmwareUpdate.cpp "../../FirmwareUpdate" "${FIRMWAREUPDATE_LIBS}")

# PLUGIN_USB_MASS_STORAGE
set (USB_MASS_STORAGE_LIBS ${NAMESPACE}USBMassStorage ${NAMESPACE}USBMassStorageImplementation)
add_plugin_test_ex(PLUGIN_USB_MASS_STORAGE tests/test_USBMassStorage.cpp "../../USBMassStorage" "${USB_MASS_STORAGE_LIBS}")

# PLUGIN_APPMANAGER
set (APPMANAGER_LIBS ${NAMESPACE}AppManager ${NAMESPACE}AppManagerImplementation)
add_plugin_test_ex(PLUGIN_APPMANAGER tests/test_AppManager.cpp "../../AppManager" "${APPMANAGER_LIBS}")

if(PLUGIN_POWERMANAGER)
    # test src file
    list(APPEND TEST_SRC ../mocks/PowerManagerMock.cpp)
    # includes
    list(APPEND TEST_INC ../../PowerManager)
endif()


add_executable(${PROJECT_NAME}
        ${TEST_SRC}
        ../mocks/Rfc.cpp
        ../mocks/Iarm.cpp
        ../mocks/RBus.cpp
        ../mocks/MotionDetection.cpp
        ../mocks/Telemetry.cpp
        ../mocks/Udev.cpp
        ../mocks/devicesettings.cpp
        ../mocks/HdmiCec.cpp
        ../mocks/thunder/Module.cpp
        ../mocks/Wraps.cpp
        ../mocks/Dobby.cpp
        ../mocks/rdkshell.cpp
        ../mocks/opkgMock.cpp
        ../mocks/readprocMockInterface.cpp
        ../mocks/libusb.cpp
        $<$<BOOL:${USE_THUNDER_R4}>:../mocks/thunder/Communicator.cpp>
  )

set_source_files_properties(
        tests/test_DeviceAudioCapabilities.cpp
        tests/test_DeviceVideoCapabilities.cpp
        tests/test_SystemServices.cpp
        tests/test_Warehouse.cpp
        tests/test_HdmiCec2.cpp
        tests/test_HdmiCecSource.cpp
        tests/test_FrontPanel.cpp
        tests/test_HdmiCecSink.cpp
        PROPERTIES COMPILE_FLAGS "-fexceptions")

include_directories(${TEST_INC})

target_link_libraries(${PROJECT_NAME} ${TEST_LIB})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ../mocks
        ../mocks/devicesettings
        ../mocks/thunder
        ${CMAKE_CURRENT_SOURCE_DIR}/../../OCIContainer/stubs
        ../../../Thunder/Source/plugins
        )

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
