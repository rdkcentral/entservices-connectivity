#include "ResourceManager.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0  
#define API_VERSION_NUMBER_PATCH 0

using namespace WPEFramework;

const string WPEFramework::Plugin::ResourceManager::SERVICE_NAME = "org.rdk.ResourceManagerTop";

const string WPEFramework::Plugin::ResourceManager::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";
const string WPEFramework::Plugin::ResourceManager::METHOD_GET_SYSTEM_RESOURCE_INFO = "getSystemResourceInfo";
const string WPEFramework::Plugin::ResourceManager::METHOD_GET_STATE = "getState";


namespace WPEFramework {
    namespace {
        static Plugin::Metadata<Plugin::ResourceManager> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin {

        SERVICE_REGISTRATION(
            ResourceManager,
            API_VERSION_NUMBER_MAJOR,
            API_VERSION_NUMBER_MINOR,
            API_VERSION_NUMBER_PATCH
        );

        //constructor
        ResourceManager::ResourceManager()
            : PluginHost::JSONRPC()
            , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
            , _service(nullptr)
        {
            Register(METHOD_GET_API_VERSION_NUMBER, &ResourceManager::getApiVersionNumber, this);
            Register(METHOD_GET_SYSTEM_RESOURCE_INFO, &ResourceManager::getSystemResourceInfo, this);
            Register(METHOD_GET_STATE, &ResourceManager::getState, this);
        }

        //Destructor
        ResourceManager::~ResourceManager() {}

        /////////////IMPLEMENT LIFECYCLE METHODS//////////////////////

        const string ResourceManager::Initialize(PluginHost::IShell* shell)
        {
            ASSERT(shell != nullptr);
            _service = shell;
            _service->AddRef();
            LOGINFO("ResourceManager initialized");
            return {};
        }

        const string ResourceManager::Information() const
        {
            return "{\"service\": \"" + SERVICE_NAME + "\"}";
        }

        void ResourceManager::Deinitialize(PluginHost::IShell* service)
        {
            if (_service != nullptr) {
                _service->Release();
                _service = nullptr;
            }
            LOGINFO("ResourceManager deinitialized");
        }

        //////////////IMPLEMENTATION OF INTERNAL LOGIC///////////

        // Runs "top -n 1 -b | head" and returns its output as a string
        string ResourceManager::exec_top()
        {
            char buffer[256];
            string result;
            FILE* pipe = popen("top -n 1 -b | head", "r");
            if (!pipe) {
                LOGERR("popen failed: unable to run top command");
                return "";
            }
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            LOGINFO("exec_top completed, output size: %zu bytes", result.size());
            return result;
        }

        ///////IMPLEMENTATION OF REGISTERED METHODS///////////

        uint32_t ResourceManager::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            response["version"] = m_apiVersionNumber;
            response["success"] = true;
            return Core::ERROR_NONE;
        }

        uint32_t ResourceManager::getState(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            if (_service == nullptr) {
                LOGERR("getState called but service is not initialized");
                response["success"] = false;
                response["message"] = "Service not initialized";
                return Core::ERROR_GENERAL;
            }

            response["state"] = "active";
            response["success"] = true;
            LOGINFO("getState: plugin is active");
            return Core::ERROR_NONE;
        }

        uint32_t ResourceManager::getSystemResourceInfo(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            if (_service == nullptr) {
                LOGERR("getSystemResourceInfo called but service is not initialized");
                response["success"] = false;
                response["message"] = "Service not initialized";
                return Core::ERROR_GENERAL;
            }

            LOGINFO("getSystemResourceInfo: running top command");
            string topOutput = exec_top();
            if (topOutput.empty()) {
                LOGERR("getSystemResourceInfo: top command returned empty output");
                response["success"] = false;
                response["message"] = "Failed to retrieve system resource info";
                return Core::ERROR_GENERAL;
            }

            response["resourceInfo"] = topOutput;
            response["success"] = true;
            LOGINFO("getSystemResourceInfo: success");
            return Core::ERROR_NONE;
        }

    } // namespace Plugin
} // namespace WPEFramework
    


