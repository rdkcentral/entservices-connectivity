/**
 * Calculator Plugin
 */

#pragma once

#include "Module.h"
#include <cmath>

namespace WPEFramework {
namespace Plugin {

class Calc : public PluginHost::IPlugin, public PluginHost::JSONRPC {

private:
    // Prevent copy
    Calc(const Calc&) = delete;
    Calc& operator=(const Calc&) = delete;

    // JSONRPC Registered Methods
    uint32_t getApiVersionNumber(const JsonObject& parameters, JsonObject& response);
    uint32_t addWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t subtractWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t multiplyWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t divideWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t modulusWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t powerWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t sqrtWrapper(const JsonObject& parameters, JsonObject& response);

private: // Internal logic
    double add(double a, double b);
    double subtract(double a, double b);
    double multiply(double a, double b);
    double divide(double a, double b);
    int modulus(int a, int b);
    double power(double a, double b);
    double sqrtValue(double a);

public:
    // Service Name
    static const string SERVICE_NAME;

    // Methods
    static const string METHOD_ADD;
    static const string METHOD_SUBTRACT;
    static const string METHOD_MULTIPLY;
    static const string METHOD_DIVIDE;
    static const string METHOD_MODULUS;
    static const string METHOD_POWER;
    static const string METHOD_SQRT;
    static const string METHOD_GET_API_VERSION_NUMBER;

    Calc();
    virtual ~Calc();

    virtual const string Initialize(PluginHost::IShell* shell) override { return {}; }
    virtual void Deinitialize(PluginHost::IShell* service) override;
    virtual string Information() const override;

    BEGIN_INTERFACE_MAP(Calc)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
    END_INTERFACE_MAP

private:
    uint32_t m_apiVersionNumber;
};

} // Plugin
} // WPEFramework
