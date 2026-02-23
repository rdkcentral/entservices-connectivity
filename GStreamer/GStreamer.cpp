#include "GStreamer.h"

using namespace WPEFramework;

namespace WPEFramework {
    namespace {
        static Plugin::Metadata<GStreamer> metadata(
            // Version of the plugin, not to be confused with the API version
            1, 0, 0,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin {
        SERVICE_REGISTRATION(GStreamer, 1, 0);


        const string GStreamer::SERVICE_NAME = "org.rdk.GStreamer";

        const string GStreamer::METHOD_PLAY = "play";
        const string GStreamer::METHOD_PAUSE = "pause";
        const string GStreamer::METHOD_QUIT = "quit";
        const string GStreamer::METHOD_GET_API_VERSION_NUMBER = "getApiVersion";

        GStreamer::GStreamer()
            : PluginHost::JSONRPC()
            , m_apiVersionNumber(1) // Start with API version 1
            {
                Register(METHOD_PLAY, &GStreamer::playWrapper, this);
                Register(METHOD_PAUSE, &GStreamer::pauseWrapper, this);
                Register(METHOD_QUIT, &GStreamer::quitWrapper, this);
                Register(METHOD_GET_API_VERSION_NUMBER, &GStreamer::getApiVersionNumberWrapper, this);  
            }

            GStreamer::~GStreamer() {
                // Cleanup if necessary
            }

            void GStreamer::Deinitialize(PluginHost::IShell* service) {
                // Cleanup if necessary
            }


            //Internal Logic for the GStreamer plugin
    }
}