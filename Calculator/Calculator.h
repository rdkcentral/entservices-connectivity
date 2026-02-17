#pragma once

#include "Module.h"
#include "include/calc.h"

namespace WPEFramework {
    namespace Plugin {
        class Calculator : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        public:
            Calculator();
            virtual ~Calculator();
            virtual const string Initialize(PluginHost::IShell* shell) override { return {}; }
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override;

            // JSON-RPC method handlers
            uint32_t addWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t subWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t mulWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t divWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t modWrapper(const JsonObject& parameters, JsonObject& response);

            BEGIN_INTERFACE_MAP(Calculator)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP
        };
    }
}
