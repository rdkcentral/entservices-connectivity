#include "ResourceManagerTop.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0  
#define API_VERSION_NUMBER_PATCH 0

using namespace WPEFramework;

const string WPEFramework::Plugin::ResourceManagerTop::SERVICE_NAME = "org.rdk.ResourceManagerTop";

const string WPEFramework::Plugin::ResourceManagerTop::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";
const string WPEFramework::Plugin::ResourceManagerTop::METHOD_GET_SYSTEM_RESOURCE_INFO = "getSystemResourceInfo";
const string WPEFramework::Plugin::ResourceManagerTop::METHOD_GET_STATE = "getState";


namespace WPEFramework {
    namespace {
        static Plugin::Metadata<Plugin::ResourceManagerTop> metadata(
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
            ResourceManagerTop,
            API_VERSION_NUMBER_MAJOR,
            API_VERSION_NUMBER_MINOR,
            API_VERSION_NUMBER_PATCH
        );

        //constructor
        ResourceManagerTop::ResourceManagerTop()
            : PluginHost::JSONRPC()
            , m_apiVersionNumber(API_VERSION_NUMBER_MAJOR)
            , _service(nullptr)
        {
            Register(METHOD_GET_API_VERSION_NUMBER, &ResourceManagerTop::getApiVersionNumber, this);
            Register(METHOD_GET_SYSTEM_RESOURCE_INFO, &ResourceManagerTop::getSystemResourceInfo, this);
            Register(METHOD_GET_STATE, &ResourceManagerTop::getState, this);
        }

        //Destructor
        ResourceManagerTop::~ResourceManagerTop() {}

        /////////////IMPLEMENT LIFECYCLE METHODS//////////////////////

        const string ResourceManagerTop::Initialize(PluginHost::IShell* shell)
        {
            ASSERT(shell != nullptr);
            _service = shell;
            _service->AddRef();
            LOGINFO("ResourceManager initialized");
            return {};
        }

        string ResourceManagerTop::Information() const
        {
            return "{\"service\": \"" + SERVICE_NAME + "\"}";
        }

        void ResourceManagerTop::Deinitialize(PluginHost::IShell* service)
        {
            if (_service != nullptr) {
                _service->Release();
                _service = nullptr;
            }
            LOGINFO("ResourceManager deinitialized");
        }

        //////////////IMPLEMENTATION OF INTERNAL LOGIC///////////

        // Runs "top -n 1 -b | head" and returns its output as a string
        string ResourceManagerTop::exec_top()
        {
            char buffer[256];
            string result;
            FILE* pipe = popen("top -n 1 -b | head -n 20", "r");
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
        bool kill_process(const int& pid)
        {
            if(pid<0)
            {
                LOGERR("INVALID PID: %d", pid);
                return false;
            }
            int ret = kill(pid, SIGKILL);
            if(ret == 0)
            {
                LOGINFO("Process with PID %d killed successfully", pid);
                return true;
            }
            else
            {
                LOGERR("Failed to kill process with PID %d, error: %s", pid, strerror(errno));
                return false;
            }
        }
        bool kil_process_by_name(const string& processName)
        {
            if(processName.empty())
            {
                LOGERR("Process name is empty");
                return false;
            }
            string command = "pkill -9 " + processName;
            int ret = system(command.c_str());
            if(ret == 0)
            {
                LOGINFO("Process with name %s killed successfully", processName.c_str());
                return true;
            }
            else
            {
                LOGERR("Failed to kill process with name %s, error: %s", processName.c_str(), strerror(errno));
                return false;
            }
        }

        ///////IMPLEMENTATION OF REGISTERED METHODS///////////

        uint32_t ResourceManagerTop::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            response["version"] = m_apiVersionNumber;
            response["success"] = true;
            return Core::ERROR_NONE;
        }

        uint32_t ResourceManagerTop::getState(const JsonObject& parameters, JsonObject& response)
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

        uint32_t ResourceManagerTop::getSystemResourceInfo(const JsonObject& parameters, JsonObject& response)
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

        uint32_t ResourceManagerTop::killProcess(const JsonObject& parameters, JsonObject& response)
        {
            if(parameters.HasLabel("pid"))
            {
                int pid;
                getNumberParameter("pid", pid);
                if(kill_process(pid))
                {
                    response["success"] = true;
                    response["message"] = "Process killed successfully";
                    return Core::ERROR_NONE;
                }
                else
                {
                    response["success"] = false;
                    response["message"] = "Failed to kill process";
                    return Core::ERROR_GENERAL;
                }
            }
            else if(parameters.HasLabel("processName"))
            {
                string processName;
                getStringParameter("processName", processName);
                if(kill_process_by_name(processName))
                {
                    response["success"] = true;
                    response["message"] = "Process killed successfully";
                    return Core::ERROR_NONE;
                }
                else
                {
                    response["success"] = false;
                    response["message"] = "Failed to kill process";
                    return Core::ERROR_GENERAL;
                }
            }
            else
            {
                response["success"] = false;
                response["message"] = "Missing required parameter: pid or processName";
                return Core::ERROR_BAD_REQUEST;
            }
        }

    } // namespace Plugin
} // namespace WPEFramework
    


