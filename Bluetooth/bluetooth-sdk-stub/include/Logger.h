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
#include <map>
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
  void log(std::string level, Args... args) {
    if (m_output == LogOutput::Stdout) {
      std::cout << currentTime() << " - " << level << ": ";
      (std::cout << ... << args) << std::endl;
    } else if (m_output == LogOutput::File) {
      m_logFile << currentTime() << " - " << level << ": ";
      (m_logFile << ... << args) << std::endl;
    } else if (m_output == LogOutput::LogRedirect)
    {
      //convert args to string
      std::ostringstream oss;
      (oss << ... << args);
      std::string logMsg = oss.str();
      switch (level[0]) {
        case 'D':
          m_logRedirect->m_debug(logMsg);
          break;
        case 'I':
          m_logRedirect->m_info(logMsg);
          break;
        case 'W':
          m_logRedirect->m_warning(logMsg);
          break;
        case 'E':
          m_logRedirect->m_error(logMsg);
          break;
        default:
          break;
      }
    }
  }
  bool setLogOutput(LogOutput output, std::variant<std::string, std::unique_ptr<LogRedirect>> outputParam) {
    // Reset previous output if changing
    if (m_output == LogOutput::File && output != LogOutput::File)
    {
      if (m_logFile.is_open())
      {
        m_logFile.close();
      }
    }
    if (m_output == LogOutput::LogRedirect && output != LogOutput::LogRedirect)
    {
      m_logRedirect.reset();
    }
    // Set new output
    if (output == LogOutput::File && m_output != LogOutput::File) {
      m_logFile.open(std::get<std::string>(outputParam), std::ios::app);
      if (!m_logFile.is_open()) {
        return false;
      }
      m_output = LogOutput::File;
      return true;
    }
    else if (output == LogOutput::LogRedirect && m_output != LogOutput::LogRedirect)
    {
      m_logRedirect = std::move(std::get<std::unique_ptr<LogRedirect>>(outputParam));
      m_output = LogOutput::LogRedirect;
      return true;
    }
    else if (output == LogOutput::Stdout && m_output != LogOutput::Stdout)
    {
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
  LogOutput m_output = LogOutput::Stdout;
  std::ofstream m_logFile;
  std::unique_ptr<LogRedirect> m_logRedirect;
  std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);
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

