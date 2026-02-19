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
 
////////////////////////////////////////////////////////////
 
Calculator::Calculator()
    : PluginHost::JSONRPC()
    , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
{
    Register(METHOD_GET_API_VERSION_NUMBER, &Calculator::getApiVersionNumber, this);
    Register(METHOD_ADD, &Calculator::addWrapper, this);
    Register(METHOD_SUBTRACT, &Calculator::subtractWrapper, this);
    Register(METHOD_MULTIPLY, &Calculator::multiplyWrapper, this);
    Register(METHOD_DIVIDE, &Calculator::divideWrapper, this);
    Register(METHOD_MODULUS, &Calculator::modulusWrapper, this);
    Register(METHOD_POWER, &Calculator::powerWrapper, this);
    Register(METHOD_SQRT, &Calculator::sqrtWrapper, this);
}
 
Calculator::~Calculator() {}
 
////////////////////////////////////////////////////////////
 
void Calculator::Deinitialize(PluginHost::IShell* service)
{
    UNUSED(service);
}
 
string Calculator::Information() const
{
    return string("{\"service\": \"") + SERVICE_NAME + "\"}";
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
    UNUSED(parameters);
    response["version"] = m_apiVersionNumber;
    returnResponse(true);
}
 
uint32_t Calculator::addWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = add(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = subtract(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = multiply(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::divideWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    if (b == 0) {
        returnResponse(false);
    }
 
    response["result"] = divide(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::modulusWrapper(const JsonObject& parameters, JsonObject& response)
{
    int a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    if (b == 0) {
        returnResponse(false);
    }
 
    response["result"] = modulus(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = power(a, b);
    returnResponse(true);
}
 
uint32_t Calculator::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a;
    getNumberParameterObject(parameters, "a", a);
 
    if (a < 0) {
        returnResponse(false);
    }
 
    response["result"] = sqrtValue(a);
    returnResponse(true);
}
 
} // Plugin
} // WPEFramework
