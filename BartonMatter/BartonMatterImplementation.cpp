/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
**/

#include "BartonMatterImplementation.h"

using namespace std;

namespace WPEFramework
{
    namespace Plugin
    {

        std::string gPendingIdRequest("");
        std::string gPendingIdOptionsRequest("");
	std::string gPendingUrl("");
	SERVICE_REGISTRATION(BartonMatterImplementation, 1, 0);

        BartonMatterImplementation::BartonMatterImplementation()
        {
            TRACE(Trace::Information, (_T("Constructing BartonMatterImplementation Service: %p"), this));
        }

        BartonMatterImplementation::~BartonMatterImplementation()
        {
            TRACE(Trace::Information, (_T("Destructing BartonMatterImplementation Service: %p"), this));
        }

	Core::hresult BartonMatterImplementation::CreateApplication(const std::string options, uint32_t& id)
	{
		return (Core::ERROR_NONE);
	}

	Core::hresult BartonMatterImplementation::RunApplication(uint32_t id, const std::string url)
	{
		LOGINFO("runApplication invoked");
		return (Core::ERROR_NONE);
	}

	Core::hresult BartonMatterImplementation::RunJavaScript(uint32_t id, const std::string code)
	{
		LOGINFO("runJavaScript invoked");
		return (Core::ERROR_NONE);
	}

	Core::hresult BartonMatterImplementation::GetApplications()
	{
		LOGINFO("getApplication invoked");
		return (Core::ERROR_NONE);
	}

	Core::hresult BartonMatterImplementation::TerminateApplication(uint32_t id)
	{
		LOGINFO("terminateApplication invoked");
		return (Core::ERROR_NONE);
	}
} // namespace Plugin
} // namespace WPEFramework
