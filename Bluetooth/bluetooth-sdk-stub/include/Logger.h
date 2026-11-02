/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <map>
#include <mutex>
#include <variant>

#include "LogRedirect.h"

enum class LogOutput { Stdout, File, LogRedirect }; //File is for logging to a file, LogRedirect is for logging to given callbacks
// singleton logger class
class Logger {
 public:
  static Logger& getLogger() {
    static Logger logger;
    return logger;
  }
  template <typename... Args>
  void log(const std::string& level, Args&&... args) {
    // Format outside the lock; the lock is recursive because a redirect callback may log.
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::string logMsg = oss.str();
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_output == LogOutput::Stdout) {
      std::cout << currentTime() << " - " << level << ": " << logMsg << std::endl;
    } else if (m_output == LogOutput::File) {
      m_logFile << currentTime() << " - " << level << ": " << logMsg << std::endl;
    } else if (m_output == LogOutput::LogRedirect && m_logRedirect)
    {
      switch (level[0]) {
        case 'D':
          if (m_logRedirect->m_debug) m_logRedirect->m_debug(logMsg);
          break;
        case 'I':
          if (m_logRedirect->m_info) m_logRedirect->m_info(logMsg);
          break;
        case 'W':
          if (m_logRedirect->m_warning) m_logRedirect->m_warning(logMsg);
          break;
        case 'E':
          if (m_logRedirect->m_error) m_logRedirect->m_error(logMsg);
          break;
        default:
          break;
      }
    }
  }
  bool setLogOutput(LogOutput output, std::variant<std::string, std::unique_ptr<LogRedirect>> outputParam = {}) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    // The new sink is validated before the previous one is released, so a failure leaves the
    // current output untouched.
    if (output == LogOutput::File) {
      const std::string* path = std::get_if<std::string>(&outputParam);
      if (path == nullptr) {
        return false;
      }
      std::ofstream file(*path, std::ios::app);
      if (!file.is_open()) {
        return false;
      }
      if (m_logFile.is_open()) {
        m_logFile.close();
      }
      m_logFile = std::move(file);
      m_logRedirect.reset();
      m_output = LogOutput::File;
    }
    else if (output == LogOutput::LogRedirect)
    {
      auto* redirect = std::get_if<std::unique_ptr<LogRedirect>>(&outputParam);
      if (redirect == nullptr || !*redirect) {
        return false;
      }
      if (m_logFile.is_open()) {
        m_logFile.close();
      }
      m_logRedirect = std::move(*redirect);
      m_output = LogOutput::LogRedirect;
    }
    else
    {
      if (m_logFile.is_open()) {
        m_logFile.close();
      }
      m_logRedirect.reset();
      m_output = LogOutput::Stdout;
    }
    return true;
  }

 private:
  explicit Logger() {}
  ~Logger() {
    if (m_logFile.is_open()) {
      m_logFile.close();
    }
    
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  std::recursive_mutex m_mutex;
  LogOutput m_output = LogOutput::Stdout;
  std::ofstream m_logFile;
  std::unique_ptr<LogRedirect> m_logRedirect;
  std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm{};
    if (localtime_r(&now_time, &now_tm) == nullptr) {
      return "0000-00-00 00:00:00";
    }
    char buffer[20];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &now_tm) == 0) {
      return "0000-00-00 00:00:00";
    }
    return std::string(buffer);
  }
};

#define DBG(...) Logger::getLogger().log("DEBUG", __VA_ARGS__)
#define INFO(...) Logger::getLogger().log("INFO", __VA_ARGS__)
#define WARN(...) Logger::getLogger().log("WARN", __VA_ARGS__)
#define ERR(...) Logger::getLogger().log("ERROR", __VA_ARGS__)
#define SET_LOG_OUTPUT_TO_FILE(fp) Logger::getLogger().setLogOutput(LogOutput::File, fp)
#define SET_LOG_OUTPUT_TO_REDIRECT(redirect) Logger::getLogger().setLogOutput(LogOutput::LogRedirect, std::move(redirect))
#define SET_LOG_OUTPUT_TO_STDOUT() Logger::getLogger().setLogOutput(LogOutput::Stdout)

