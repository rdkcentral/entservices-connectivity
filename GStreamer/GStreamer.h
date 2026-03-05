#pragma once

#include "Module.h"
#include <gst/gst.h>

namespace WPEFramework {
namespace Plugin {

class GStreamer : public PluginHost::IPlugin, public PluginHost::JSONRPC {

private:
    // Prevent copy and assignment
    GStreamer(const GStreamer&) = delete;
    GStreamer& operator=(const GStreamer&) = delete;

    // JSONRPC wrapper methods
    uint32_t initializeWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t playWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t pauseWrapper(const JsonObject& parameters, JsonObject& response);
    uint32_t quitWrapper(const JsonObject& parameters, JsonObject& response);

private:
    // Internal logic for the GStreamer plugin
    string buildPipeline();
    string playPipeline();
    string pausePipeline();
    void cleanupPipeline();
    
    // Pad-added handler for dynamic pads
    static void padAddedHandler(GstElement* src, GstPad* newPad, GStreamer* data);

public:
    // Service Name
    static const string SERVICE_NAME;

    // Methods
    static const string METHOD_INITIALIZE;
    static const string METHOD_PLAY;
    static const string METHOD_PAUSE;
    static const string METHOD_QUIT;

    GStreamer();
    virtual ~GStreamer() override;

    virtual const string Initialize(PluginHost::IShell* shell) override;
    virtual void Deinitialize(PluginHost::IShell* service) override;
    virtual string Information() const override;

    BEGIN_INTERFACE_MAP(GStreamer)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
    END_INTERFACE_MAP

private:
    // GStreamer pipeline elements
    GstElement* m_pipeline;
    GstElement* m_source;
    GstElement* m_audioconvert;
    GstElement* m_audioresample;
    GstElement* m_audiosink;
    GstElement* m_videoconvert;
    GstElement* m_videosink;
    GstBus* m_bus;
    
    bool m_pipelineInitialized;
};

} // Plugin
} // WPEFramework