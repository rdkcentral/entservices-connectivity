#pragma once

#include "Module.h"

namespace WPEFramework{
    namespace Plugin {
        class GStreamer : public PluginHost::IPlugin, public PluginHost::JSONRPC {

            #To prevent copy and assignment
            private:    
                GStreamer(const GStreamer&) = delete;
                GStreamer& operator=(const GStreamer&) = delete;
            
                //JSONRPC methods
                bool playWrapper(const JsonObject& parameters, JsonObject& response);
                bool pauseWrapper(const JsonObject& parameters, JsonObject& response);
                bool quitWrapper(const JsonObject& parameters, JsonObject& response);
            
            //internal logic for the Gstreamer plugin
            private:
                void play();
                void pause();
                void quit();
            
            public:
                static const string SERVICE_NAME;

                static const string METHOD_PLAY;
                static const string METHOD_PAUSE;  
                static const string METHOD_QUIT;

                static const string METHOD_GET_API_VERSION_NUMBER;

                GStreamer();
                virtual ~GStreamer() override;

                virtual const string Initialize(PluginHost::IShell* shell) override { return {}; }
                virtual void Deinitialize(PluginHost::IShell* service) override;
                virtual string Information() const override;

                BEGIN_INTERFACE_MAP(GStreamer)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                END_INTERFACE_MAP

                private:
                    uint32_t m_apiVersionNumber;
        };
    }//plugin
}//namespace WPEFramework