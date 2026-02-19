<<<<<<< HEAD
<<<<<<< HEAD
/**
 * Calculator Plugin Implementation
 */

#include "Calc.h"

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

// Static strings
const string Calc::SERVICE_NAME = "org.rdk.Calculator";

const string Calc::METHOD_ADD = "add";
const string Calc::METHOD_SUBTRACT = "subtract";
const string Calc::METHOD_MULTIPLY = "multiply";
const string Calc::METHOD_DIVIDE = "divide";
const string Calc::METHOD_MODULUS = "modulus";
const string Calc::METHOD_POWER = "power";
const string Calc::METHOD_SQRT = "sqrt";
const string Calc::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";

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

void Calc::Deinitialize(PluginHost::IShell* service)
{
    UNUSED(service);
}

string Calc::Information() const
{
    return string("{\"service\": \"") + SERVICE_NAME + "\"}";
}

///////////////////////////////////////////////////////////
//////////////////// INTERNAL LOGIC ///////////////////////
///////////////////////////////////////////////////////////

double Calc::add(double a, double b) { return a + b; }
double Calc::subtract(double a, double b) { return a - b; }
double Calc::multiply(double a, double b) { return a * b; }
double Calc::divide(double a, double b) { return (b != 0) ? a / b : 0; }
int Calc::modulus(int a, int b) { return (b != 0) ? a % b : 0; }
double Calc::power(double a, double b) { return std::pow(a, b); }
double Calc::sqrtValue(double a) { return std::sqrt(a); }

///////////////////////////////////////////////////////////
//////////////////// WRAPPERS /////////////////////////////
///////////////////////////////////////////////////////////

uint32_t Calc::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
{
    UNUSED(parameters);
    response["version"] = m_apiVersionNumber;
    returnResponse(true);
}

uint32_t Calc::addWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    response["result"] = add(a, b);
    returnResponse(true);
}

uint32_t Calc::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    response["result"] = subtract(a, b);
    returnResponse(true);
}

uint32_t Calc::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    response["result"] = multiply(a, b);
    returnResponse(true);
}

uint32_t Calc::divideWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    if (b == 0)
        returnResponse(false);

    response["result"] = divide(a, b);
    returnResponse(true);
}

uint32_t Calc::modulusWrapper(const JsonObject& parameters, JsonObject& response)
{
    int a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    if (b == 0)
        returnResponse(false);

    response["result"] = modulus(a, b);
    returnResponse(true);
}

uint32_t Calc::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);

    response["result"] = power(a, b);
    returnResponse(true);
}

uint32_t Calc::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a;
    getNumberParameterObject(parameters, "a", a);

    if (a < 0)
        returnResponse(false);

    response["result"] = sqrtValue(a);
    returnResponse(true);
}

} // Plugin
} // WPEFramework
=======
/**
* Calculator Plugin Implementation
*/
 
#include "Calc.h"
 
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
 
// Service name
const string Calc::SERVICE_NAME = "org.rdk.Calculator";
 
// Method names
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
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = add(a, b);
    returnResponse(true);
}
 
uint32_t Calc::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = subtract(a, b);
    returnResponse(true);
}
 
uint32_t Calc::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = multiply(a, b);
    returnResponse(true);
}
 
uint32_t Calc::divideWrapper(const JsonObject& parameters, JsonObject& response)
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
 
uint32_t Calc::modulusWrapper(const JsonObject& parameters, JsonObject& response)
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
 
uint32_t Calc::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    double a, b;
    getNumberParameterObject(parameters, "a", a);
    getNumberParameterObject(parameters, "b", b);
 
    response["result"] = power(a, b);
    returnResponse(true);
}
 
uint32_t Calc::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
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
>>>>>>> a7e492f1e30e760d8fee5e67975f7b488fd84901
=======
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

const string Calc::SERVICE_NAME = "org.rdk.Calculator";

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

void Calc::Deinitialize(PluginHost::IShell* service)
{
    UNUSED(service);
}

string Calc::Information() const
{
    return string("{\"service\": \"") + SERVICE_NAME + "\"}";
}

////////////////////////////////////////////////////////////
// INTERNAL LOGIC
////////////////////////////////////////////////////////////

double Calc::add(double a, double b) { return a + b; }
double Calc::subtract(double a, double b) { return a - b; }
double Calc::multiply(double a, double b) { return a * b; }
double Calc::divide(double a, double b) { return a / b; }
int Calc::modulus(int a, int b) { return a % b; }
double Calc::power(double a, double b) { return std::pow(a, b); }
double Calc::sqrtValue(double a) { return std::sqrt(a); }

////////////////////////////////////////////////////////////
// WRAPPERS (CORRECT THUNDER STYLE)
////////////////////////////////////////////////////////////

uint32_t Calc::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
{
    UNUSED(parameters);
    response["version"] = m_apiVersionNumber;
    return Core::ERROR_NONE;
}

uint32_t Calc::addWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = add(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::subtractWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = subtract(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::multiplyWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = multiply(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::divideWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    if (b == 0)
        return Core::ERROR_GENERAL;

    response["result"] = divide(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::modulusWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    int a = static_cast<int>(parameters["a"].Number());
    int b = static_cast<int>(parameters["b"].Number());

    if (b == 0)
        return Core::ERROR_GENERAL;

    response["result"] = modulus(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::powerWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a") || !parameters.HasLabel("b"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();
    double b = parameters["b"].Number();

    response["result"] = power(a, b);
    return Core::ERROR_NONE;
}

uint32_t Calc::sqrtWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!parameters.HasLabel("a"))
        return Core::ERROR_GENERAL;

    double a = parameters["a"].Number();

    if (a < 0)
        return Core::ERROR_GENERAL;

    response["result"] = sqrtValue(a);
    return Core::ERROR_NONE;
}

} // Plugin
} // WPEFramework
>>>>>>> 99d010f958434a058fb8ade63d442dc210251f24
