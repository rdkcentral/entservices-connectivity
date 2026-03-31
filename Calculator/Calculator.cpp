// This file was renamed from Calc.cpp to Calculator.cpp for naming consistency.
// The content is identical to the updated Calc.cpp.
/**
* Calculator Plugin Implementation
*/
 
#include "Calculator.h"
 
#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0
 
using namespace WPEFramework;
 
namespace WPEFramework {
 
namespace {
    static Plugin::Metadata<Plugin::Calculator> metadata(
        API_VERSION_NUMBER_MAJOR,
        API_VERSION_NUMBER_MINOR,
        API_VERSION_NUMBER_PATCH,
        {},
        {},
        {}
    );
}
 
namespace Plugin {
 
SERVICE_REGISTRATION(Calculator,
                     API_VERSION_NUMBER_MAJOR,
                     API_VERSION_NUMBER_MINOR,
                     API_VERSION_NUMBER_PATCH);
 
// Service name
const string Calculator::SERVICE_NAME = "org.rdk.Calculator";
 
// Method names
const string Calculator::METHOD_ADD = "add";
const string Calculator::METHOD_SUBTRACT = "subtract";
const string Calculator::METHOD_MULTIPLY = "multiply";
const string Calculator::METHOD_DIVIDE = "divide";
const string Calculator::METHOD_MODULUS = "modulus";
const string Calculator::METHOD_POWER = "power";
const string Calculator::METHOD_SQRT = "sqrt";
const string Calculator::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";
const string Calculator::METHOD_GET_FIRM_DETAILS = "getFirmWareinfo";
 
////////////////////////////////////////////////////////////
 
Calculator::Calculator()
    : PluginHost::JSONRPC()
    , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
    , _service(nullptr)
{
    Register(METHOD_GET_API_VERSION_NUMBER, &Calculator::getApiVersionNumber, this);
    Register(METHOD_ADD, &Calculator::addWrapper, this);
    Register(METHOD_SUBTRACT, &Calculator::subtractWrapper, this);
    Register(METHOD_MULTIPLY, &Calculator::multiplyWrapper, this);
    Register(METHOD_DIVIDE, &Calculator::divideWrapper, this);
    Register(METHOD_MODULUS, &Calculator::modulusWrapper, this);
    Register(METHOD_POWER, &Calculator::powerWrapper, this);
    Register(METHOD_SQRT, &Calculator::sqrtWrapper, this);
    Register(METHOD_GET_FIRM_DETAILS, &Calculator::getFirmWareinfo, this);
}
 
Calculator::~Calculator() {}
 
////////////////////////////////////////////////////////////
 
const string Calculator::Initialize(PluginHost::IShell* shell)
{
    ASSERT(shell != nullptr);
    _service = shell;
    _service->AddRef();
    return {};
}
 
void Calculator::Deinitialize(PluginHost::IShell* service)
{
    if (_service != nullptr) {
        _service->Release();
        _service = nullptr;
    }
}
 
string Calculator::Information() const
{
    return "{\"service\": \"" + SERVICE_NAME + "\"}";
}
 
////////////////////////////////////////////////////////////
/////////////////// MAC DETAILS ////////////////////////////
////////////////////////////////////////////////////////////
 
uint32_t Calculator::getFirmWareinfo(const JsonObject& parameters, JsonObject& response)
{
    if (_service == nullptr) {
        response["success"] = false;
        response["message"] = "Plugin not initialized";
        return Core::ERROR_GENERAL;
    }

    Exchange::ISystemMode* systemMode =
        _service->QueryInterfaceByCallsign<Exchange::ISystemMode>("org.rdk.SystemMode");

    if (systemMode == nullptr) {
        response["success"] = false;
        response["message"] = "SystemMode plugin not available";
        return Core::ERROR_UNAVAILABLE;
    }

    JsonObject FWresponse;
    uint32_t status = systemMode->getDownloadedFirmwareInfo(FWresponse);

    if(status != Core::ERROR_NONE) {
        response["success"] = false;
        response["message"] = "Failed to get firmware info";
        systemMode->Release();
        return status;
    }


    //Handling the response from SystemMode plugin and adding it to our response
    JsonObject result = FWresponse["result"].Object();

    response["currentFWVersion"] = result["currentFWVersion"].String();
    response["downloadedFWVersion"] = result["downloadedFWVersion"].String();
    response["downloadedFWLocation"] = result["downloadedFWLocation"].String();
    response["isRebootDeferred"] = result["isRebootDeferred"].Boolean();
    response["success"] = result["success"].Boolean();

    systemMode->Release();
    return Core::ERROR_NONE;
}
 
////////////////////////////////////////////////////////////
//////////////////// INTERNAL LOGIC ////////////////////////
////////////////////////////////////////////////////////////
 
double Calculator::add(double a, double b) {
    return a + b;
}
 
double Calculator::subtract(double a, double b) {
    return a - b;
}
 
double Calculator::multiply(double a, double b) {
    return a * b;
}
 
double Calculator::divide(double a, double b) {
    return (b != 0) ? a / b : 0;
}
 
int Calculator::modulus(int a, int b) {
    return (b != 0) ? a % b : 0;
}
 
double Calculator::power(double a, double b) {
    return std::pow(a, b);
}
 
double Calculator::sqrtValue(double a) {
    return std::sqrt(a);
}
 
////////////////////////////////////////////////////////////
//////////////////// WRAPPER METHODS ///////////////////////
////////////////////////////////////////////////////////////
 
uint32_t Calculator::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.IsSet()) {
        response["success"] = false;
        response["message"] = "No parameters provided";
        return Core::ERROR_BAD_REQUEST;
    }
    response["version"] = m_apiVersionNumber;
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::addWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("numbers")){
     response["success"] = false;
     response["message"] = "MIssing or Invalid 'numbers' array";
     return Core::ERROR_BAD_REQUEST;
    }

    const JsonArray& numbers = parameters["numbers"].Array();

    if (numbers.Length() == 0){
      response["success"] = false;
      response["message"] = "array is empty";
      return Core::ERROR_BAD_REQUEST;
    }

    double sum=0;
    for(uint32_t i=0;i<numbers.Length();i++) {
     sum+=numbers[i].Number();
    }

    response["result"] = sum;
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    double a = parameters["a"].Number();
    double b = parameters["b"].Number();
    response["result"] = subtract(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    double a = parameters["a"].Number();
    double b = parameters["b"].Number();
    response["result"] = multiply(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::divideWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    double a = parameters["a"].Number();
    double b = parameters["b"].Number();
    if (b == 0) {
        response["success"] = false;
        response["message"] = "Division by zero";
        return Core::ERROR_BAD_REQUEST;
    }
    response["result"] = divide(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::modulusWrapper(const JsonObject& parameters, JsonObject& response)
{
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
    response["result"] = modulus(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b")) {
        response["success"] = false;
        response["message"] = "Missing parameters a or b";
        return Core::ERROR_BAD_REQUEST;
    }
    double a = parameters["a"].Number();
    double b = parameters["b"].Number();
    response["result"] = power(a, b);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
uint32_t Calculator::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a")) {
        response["success"] = false;
        response["message"] = "Missing parameter a";
        return Core::ERROR_BAD_REQUEST;
    }
    double a = parameters["a"].Number();
    if (a < 0) {
        response["success"] = false;
        response["message"] = "Square root of negative number";
        return Core::ERROR_BAD_REQUEST;
    }
    response["result"] = sqrtValue(a);
    response["success"] = true;
    return Core::ERROR_NONE;
}
 
} // Plugin
} // WPEFramework
