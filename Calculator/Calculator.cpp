#include "Calculator.h"
namespace WPEFramework {
namespace Plugin {

Calculator::Calculator() {
    Register<JsonObject, JsonObject>("add", &Calculator::addWrapper, this);
    Register<JsonObject, JsonObject>("sub", &Calculator::subWrapper, this);
    Register<JsonObject, JsonObject>("mul", &Calculator::mulWrapper, this);
    Register<JsonObject, JsonObject>("div", &Calculator::divWrapper, this);
    Register<JsonObject, JsonObject>("mod", &Calculator::modWrapper, this);
}
Calculator::~Calculator() {}

void Calculator::Deinitialize(PluginHost::IShell* service) {}
string Calculator::Information() const { return "Calculator Plugin"; }

uint32_t Calculator::addWrapper(const JsonObject& parameters, JsonObject& response) {
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    int a = parameters["a"].Number();
    int b = parameters["b"].Number();
    response["result"] = add(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}

uint32_t Calculator::subWrapper(const JsonObject& parameters, JsonObject& response) {
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    int a = parameters["a"].Number();
    int b = parameters["b"].Number();
    response["result"] = sub(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}

uint32_t Calculator::mulWrapper(const JsonObject& parameters, JsonObject& response) {
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    int a = parameters["a"].Number();
    int b = parameters["b"].Number();
    response["result"] = mul(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}

uint32_t Calculator::divWrapper(const JsonObject& parameters, JsonObject& response) {
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    int a = parameters["a"].Number();
    int b = parameters["b"].Number();
    if (b == 0) {
        response["success"] = false;
        response["message"] = "Division by zero";
        return Core::ERROR_BAD_REQUEST;
    }
    response["result"] = division(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}

uint32_t Calculator::modWrapper(const JsonObject& parameters, JsonObject& response) {
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    int a = parameters["a"].Number();
    int b = parameters["b"].Number();
    if (b == 0) {
        response["success"] = false;
        response["message"] = "Modulo by zero";
        return Core::ERROR_BAD_REQUEST;
    }
    response["result"] = mod(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}

} // namespace Plugin
} // namespace WPEFramework
