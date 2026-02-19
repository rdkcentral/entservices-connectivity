/**
 * Calculator Plugin Implementation
 */

#include "Calc.h"
#include <cmath>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

using namespace WPEFramework;

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::Calc> metadata(
        API_VERSION_NUMBER_MAJOR,
        API_VERSION_NUMBER_MINOR,
        API_VERSION_NUMBER_PATCH,
        {},
        {},
        {}
    );
}

namespace Plugin {

SERVICE_REGISTRATION(Calc,
                     API_VERSION_NUMBER_MAJOR,
                     API_VERSION_NUMBER_MINOR,
                     API_VERSION_NUMBER_PATCH);

// Service Name
const string Calc::SERVICE_NAME = "org.rdk.Calculator";

// Method Names
const string Calc::METHOD_ADD = "add";
const string Calc::METHOD_SUBTRACT = "subtract";
const string Calc::METHOD_MULTIPLY = "multiply";
const string Calc::METHOD_DIVIDE = "divide";
const string Calc::METHOD_MODULUS = "modulus";
const string Calc::METHOD_POWER = "power";
const string Calc::METHOD_SQRT = "sqrt";
const string Calc::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";

////////////////////////////////////////////////////////////

Calc::Calc()
    : PluginHost::JSONRPC()
    , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
{
    Register(METHOD_GET_API_VERSION_NUMBER, &Calc::getApiVersionNumber, this);
    Register(METHOD_ADD, &Calc::addWrapper, this);
    Register(METHOD_SUBTRACT, &Calc::subtractWrapper, this);
    Register(METHOD_MULTIPLY, &Calc::multiplyWrapper, this);
    Register(METHOD_DIVIDE, &Calc::divideWrapper, this);
    Register(METHOD_MODULUS, &Calc::modulusWrapper, this);
    Register(METHOD_POWER, &Calc::powerWrapper, this);
    Register(METHOD_SQRT, &Calc::sqrtWrapper, this);
}

Calc::~Calc() {}

////////////////////////////////////////////////////////////

void Calc::Deinitialize(PluginHost::IShell* service)
{
    UNUSED(service);
}

string Calc::Information() const
{
    return string("{\"service\": \"") + SERVICE_NAME + "\"}";
}

////////////////////////////////////////////////////////////
//////////////////// INTERNAL LOGIC ////////////////////////
////////////////////////////////////////////////////////////

double Calc::add(double a, double b) {
    return a + b;
}

double Calc::subtract(double a, double b) {
    return a - b;
}

double Calc::multiply(double a, double b) {
    return a * b;
}

double Calc::divide(double a, double b) {
    return (b != 0) ? a / b : 0;
}

int Calc::modulus(int a, int b) {
    return (b != 0) ? a % b : 0;
}

double Calc::power(double a, double b) {
    return std::pow(a, b);
}

double Calc::sqrtValue(double a) {
    return std::sqrt(a);
}

////////////////////////////////////////////////////////////
//////////////////// WRAPPER METHODS ///////////////////////
////////////////////////////////////////////////////////////

uint32_t Calc::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
{
    UNUSED(parameters);
    response["version"] = m_apiVersionNumber;
    returnResponse(true);
}

uint32_t Calc::addWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = add(a, b);
    returnResponse(true);
}

uint32_t Calc::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = subtract(a, b);
    returnResponse(true);
}

uint32_t Calc::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = multiply(a, b);
    returnResponse(true);
}

uint32_t Calc::divideWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    if (b == 0)
        returnResponse(false);

    response["result"] = divide(a, b);
    returnResponse(true);
}

uint32_t Calc::modulusWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    int a = static_cast<int>(parameters["a"].Number());
    int b = static_cast<int>(parameters["b"].Number());

    if (b == 0)
        returnResponse(false);

    response["result"] = modulus(a, b);
    returnResponse(true);
}

uint32_t Calc::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        returnResponse(false);

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = power(a, b);
    returnResponse(true);
}

uint32_t Calc::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a"))
        returnResponse(false);

    double a = parameters["a"].Number();

    if (a < 0)
        returnResponse(false);

    response["result"] = sqrtValue(a);
    returnResponse(true);
}

} // Plugin
} // WPEFramework
