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

#include <cstdio>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "BluetoothPersistenceAdapter.h"

#include "UtilsJsonRpc.h"

#ifndef BLUETOOTH_PERSISTENT_FILE_PATH
// Set at build time via -DBLUETOOTH_PERSISTENT_FILE_PATH=<path>
#define BLUETOOTH_PERSISTENT_FILE_PATH "/tmp/paired_bluetooth_devices.json"
#endif

namespace WPEFramework {
namespace Plugin {

namespace {
static const char* PERSISTENT_FILE_PATH = BLUETOOTH_PERSISTENT_FILE_PATH;
static constexpr std::streamoff kMaxFilesystemPersistencePayloadBytes = 1024 * 1024;
static std::mutex gFilesystemPersistenceWriteMutex;

bool tryParseInt64(const std::string& value, long long& parsed)
{
    if (value.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long long converted = std::strtoll(value.c_str(), &end, 10);
    if ((errno != 0) || (end == value.c_str()) || (end != nullptr && *end != '\0')) {
        return false;
    }

    parsed = converted;
    return true;
}

bool tryGetNumericAsInt64(const JsonObject& object, const char* fieldName, long long& output)
{
    if (!object.HasLabel(fieldName)) {
        return false;
    }

    const auto& value = object[fieldName];
    if (value.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
        output = static_cast<long long>(value.Number());
        return true;
    }

    if (value.Content() == WPEFramework::Core::JSON::Variant::type::STRING) {
        return tryParseInt64(value.String(), output);
    }

    return false;
}

bool tryGetBoolean(const JsonObject& object, const char* fieldName, bool& output)
{
    if (!object.HasLabel(fieldName)) {
        return false;
    }

    const auto& value = object[fieldName];
    if (value.Content() == WPEFramework::Core::JSON::Variant::type::BOOLEAN) {
        output = value.Boolean();
        return true;
    }

    if (value.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
        output = (value.Number() != 0);
        return true;
    }

    if (value.Content() == WPEFramework::Core::JSON::Variant::type::STRING) {
        std::string lowered = value.String();
        for (char& c : lowered) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lowered == "true" || lowered == "1") {
            output = true;
            return true;
        }
        if (lowered == "false" || lowered == "0") {
            output = false;
            return true;
        }
        return false;
    }

    return false;
}
}

BluetoothPersistenceAdapter::BluetoothPersistenceAdapter()
    : _filesystemPersistencePath(PERSISTENT_FILE_PATH)
{
}

Core::hresult BluetoothPersistenceAdapter::Parse(const std::string& payload, std::vector<BluetoothDeviceInfo>& devices) const
{
    JsonObject root;
    if (!root.FromString(payload)) {
        LOGERR("Failed to parse filesystem persistence file payload");
        return Core::ERROR_GENERAL;
    }

    if (!root.HasLabel("pairedDevices") ||
        root["pairedDevices"].Content() != WPEFramework::Core::JSON::Variant::type::ARRAY) {
        LOGERR("filesystem persistence file missing pairedDevices array");
        return Core::ERROR_GENERAL;
    }

    JsonArray pairedDevices = root["pairedDevices"].Array();
    
    for (uint16_t i = 0; i < pairedDevices.Length(); ++i) {
        
        JsonObject entry = pairedDevices[i].Object();
        BluetoothDeviceInfo info;

        if (entry.HasLabel("deviceAddr")) {
            info.deviceAddr = entry["deviceAddr"].String();
        }

        if (entry.HasLabel("deviceType")) {
            info.deviceType = entry["deviceType"].String();
        }

        if (entry.HasLabel("friendlyName") &&
            entry["friendlyName"].Content() == WPEFramework::Core::JSON::Variant::type::STRING) {
            info.friendlyName = entry["friendlyName"].String();
        }

        long long volumeSetting = 0;
        if (tryGetNumericAsInt64(entry, "lastVolumeSetting", volumeSetting)) {
            info.lastVolumeSetting = volumeSetting;
        }

        bool autoConnectEnabled = false;
        if (tryGetBoolean(entry, "autoConnectStatus", autoConnectEnabled)) {
            info.autoConnectStatus = autoConnectEnabled ? AUTO_CONNECT_STATUS_ENABLED : AUTO_CONNECT_STATUS_DISABLED;
        }

        long long lastConnectionUtc = 0;
        if (tryGetNumericAsInt64(entry, "lastConnectionTimeUTC", lastConnectionUtc)) {
            info.lastConnectTimeUtc = std::to_string(lastConnectionUtc);
        }

        devices.push_back(std::move(info));
    }

    return Core::ERROR_NONE;
}

Core::hresult BluetoothPersistenceAdapter::Read(std::vector<BluetoothDeviceInfo>& devices) const
{
    if (access(_filesystemPersistencePath.c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            LOGINFO("filesystem persistence file does not exist: %s", _filesystemPersistencePath.c_str());
            return Core::ERROR_NOT_EXIST;
        }
        LOGWARN("filesystem persistence file is not accessible: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }

    std::ifstream input(_filesystemPersistencePath, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        LOGWARN("filesystem persistence file is not readable: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = static_cast<std::streamoff>(input.tellg());
    if (fileSize < 0) {
        LOGWARN("filesystem persistence file size query failed: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }
    if (fileSize > kMaxFilesystemPersistencePayloadBytes) {
        LOGWARN("filesystem persistence file too large (%lld bytes), skipping parse: %s",
            static_cast<long long>(fileSize), _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }
    input.seekg(0, std::ios::beg);

    std::stringstream buffer;
    buffer << input.rdbuf();

    std::vector<BluetoothDeviceInfo> loaded;
    const Core::hresult parseResult = Parse(buffer.str(), loaded);
    if (Core::ERROR_NONE != parseResult) {
        return parseResult;
    }

    devices = std::move(loaded);
    return Core::ERROR_NONE;
}

Core::hresult BluetoothPersistenceAdapter::ReadRaw(std::string& content) const
{
    if (access(_filesystemPersistencePath.c_str(), F_OK) != 0) {
        if (errno == ENOENT) {
            LOGINFO("filesystem persistence file does not exist: %s", _filesystemPersistencePath.c_str());
            return Core::ERROR_NOT_EXIST;
        }
        LOGWARN("filesystem persistence file is not accessible: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }

    std::ifstream input(_filesystemPersistencePath, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        LOGWARN("filesystem persistence file is not readable: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = static_cast<std::streamoff>(input.tellg());
    if (fileSize < 0) {
        LOGWARN("filesystem persistence file size query failed: %s", _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }
    if (fileSize > kMaxFilesystemPersistencePayloadBytes) {
        LOGWARN("filesystem persistence file too large (%lld bytes): %s",
            static_cast<long long>(fileSize), _filesystemPersistencePath.c_str());
        return Core::ERROR_GENERAL;
    }
    input.seekg(0, std::ios::beg);

    std::stringstream buffer;
    buffer << input.rdbuf();

    if (input.bad()) {
         LOGWARN("filesystem persistence file read failed: %s", _filesystemPersistencePath.c_str());
         return Core::ERROR_GENERAL;
     }
     
    content = buffer.str();
    return Core::ERROR_NONE;
}

Core::hresult BluetoothPersistenceAdapter::Write(const std::unordered_map<std::string, BluetoothDeviceInfo>& deviceCache) const
{
    std::lock_guard<std::mutex> writeGuard(gFilesystemPersistenceWriteMutex);

    std::unordered_map<std::string, JsonObject> existingEntries;
    {
        std::ifstream input(_filesystemPersistencePath);
        if (input.is_open()) {
            input.seekg(0, std::ios::end);
            const std::streamoff fileSize = static_cast<std::streamoff>(input.tellg());
            if (fileSize > kMaxFilesystemPersistencePayloadBytes) {
                LOGWARN("filesystem persistence file too large (%lld bytes), skipping merge of existing entries: %s",
                    static_cast<long long>(fileSize), _filesystemPersistencePath.c_str());
            } else if (fileSize >= 0) {
                input.seekg(0, std::ios::beg);
                std::stringstream buffer;
                buffer << input.rdbuf();

                JsonObject existingRoot;
                if (existingRoot.FromString(buffer.str()) &&
                    existingRoot.HasLabel("pairedDevices") &&
                    existingRoot["pairedDevices"].Content() == WPEFramework::Core::JSON::Variant::type::ARRAY) {
                    JsonArray pairedDevices = existingRoot["pairedDevices"].Array();
                    for (uint16_t i = 0; i < pairedDevices.Length(); ++i) {
                        JsonObject existing = pairedDevices[i].Object();
                        if (!existing.HasLabel("deviceAddr")) {
                            continue;
                        }
                        const std::string deviceAddr = existing["deviceAddr"].String();
                        if (!deviceAddr.empty()) {
                            existingEntries[deviceAddr] = std::move(existing);
                        }
                    }
                }
            }
        }
    }

    JsonObject root;
    JsonArray pairedDevices;

    for (const auto& entry : deviceCache) {
        const std::string& deviceAddr = entry.second.deviceAddr;
        if (deviceAddr.empty()) {
            LOGWARN("Skipping device entry with no deviceAddr (deviceID=%s)", entry.first.c_str());
            continue;
        }

        auto existingIt = existingEntries.find(deviceAddr);
        JsonObject device = (existingIt != existingEntries.end()) ? existingIt->second : JsonObject();
        device["deviceAddr"] = deviceAddr;

        const std::string persistedName = !entry.second.friendlyName.empty() ? entry.second.friendlyName : deviceAddr;
        device["friendlyName"] = persistedName;

        const std::string persistedType = !entry.second.deviceType.empty() ? entry.second.deviceType : "UNKNOWN";
        device["deviceType"] = persistedType;

        device["lastVolumeSetting"] = entry.second.lastVolumeSetting;

        if (entry.second.autoConnectStatus == AUTO_CONNECT_STATUS_UNSET) {
            bool existingStatus = false;
            if (!tryGetBoolean(device, "autoConnectStatus", existingStatus)) {
                existingStatus = false;
            }
            device["autoConnectStatus"] = existingStatus;
        } else {
            device["autoConnectStatus"] = (entry.second.autoConnectStatus == AUTO_CONNECT_STATUS_ENABLED);
        }

        long long lastConnectionUtc = 0;
        if (!entry.second.lastConnectTimeUtc.empty() && tryParseInt64(entry.second.lastConnectTimeUtc, lastConnectionUtc)) {
            device["lastConnectionTimeUTC"] = lastConnectionUtc;
        } else if (tryGetNumericAsInt64(device, "lastConnectionTimeUTC", lastConnectionUtc)) {
            device["lastConnectionTimeUTC"] = lastConnectionUtc;
        } else {
            device["lastConnectionTimeUTC"] = 0;
        }

        pairedDevices.Add(device);
    }

    root["pairedDevices"] = pairedDevices;

    std::string serialized;
    root.ToString(serialized);

    const std::string tmpPath = _filesystemPersistencePath + ".tmp";
    {
        const int fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            LOGERR("Failed to open filesystem persistence temp file for write: %s", tmpPath.c_str());
            return Core::ERROR_GENERAL;
        }

        const char* data = serialized.c_str();
        size_t remaining = serialized.size();
        bool writeError = false;
        while (remaining > 0) {
            const ssize_t written = write(fd, data, remaining);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                writeError = true;
                break;
            }
            data += written;
            remaining -= static_cast<size_t>(written);
        }

        if (writeError) {
            LOGERR("Failed to write filesystem persistence temp file: %s", tmpPath.c_str());
            close(fd);
            std::remove(tmpPath.c_str());
            return Core::ERROR_GENERAL;
        }

        if (fsync(fd) != 0) {
            LOGWARN("fsync failed for temp file: %s", tmpPath.c_str());
        }
        close(fd);
    }

    if (std::rename(tmpPath.c_str(), _filesystemPersistencePath.c_str()) != 0) {
        LOGERR("Failed to atomically rename filesystem persistence file: %s -> %s", tmpPath.c_str(), _filesystemPersistencePath.c_str());
        if (std::remove(tmpPath.c_str()) != 0) {
            LOGWARN("Failed to remove temp file after failed rename: %s", tmpPath.c_str());
        }
        return Core::ERROR_GENERAL;
    }

    // fsync parent directory to make the rename entry durable
    {
        const std::string::size_type slashPos = _filesystemPersistencePath.rfind('/');
        const std::string dirPath = (slashPos != std::string::npos) ? _filesystemPersistencePath.substr(0, slashPos) : std::string(".");
        const int dirFd = open(dirPath.c_str(), O_RDONLY);
        if (dirFd < 0) {
            LOGWARN("Failed to open parent directory for fsync: %s", dirPath.c_str());
        } else {
            if (fsync(dirFd) != 0) {
                LOGWARN("fsync failed for parent directory: %s", dirPath.c_str());
            }
            close(dirFd);
        }
    }

    return Core::ERROR_NONE;
}

} // namespace Plugin
} // namespace WPEFramework
