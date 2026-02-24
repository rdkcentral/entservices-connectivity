/**
 * GStreamer Plugin Implementation
 */

#include "GStreamer.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

using namespace WPEFramework;

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::GStreamer> metadata(
        API_VERSION_NUMBER_MAJOR,
        API_VERSION_NUMBER_MINOR,
        API_VERSION_NUMBER_PATCH,
        {},
        {},
        {}
    );
}

namespace Plugin {

SERVICE_REGISTRATION(GStreamer,
                     API_VERSION_NUMBER_MAJOR,
                     API_VERSION_NUMBER_MINOR,
                     API_VERSION_NUMBER_PATCH);

// Service name
const string GStreamer::SERVICE_NAME = "org.rdk.GStreamer";

// Method names
const string GStreamer::METHOD_PLAY = "play";
const string GStreamer::METHOD_PAUSE = "pause";
const string GStreamer::METHOD_QUIT = "quit";

////////////////////////////////////////////////////////////

GStreamer::GStreamer()
    : PluginHost::JSONRPC()
    , m_pipeline(nullptr)
    , m_source(nullptr)
    , m_audioconvert(nullptr)
    , m_audioresample(nullptr)
    , m_audiosink(nullptr)
    , m_videoconvert(nullptr)
    , m_videosink(nullptr)
    , m_bus(nullptr)
    , m_pipelineInitialized(false)
{
    Register(METHOD_PLAY, &GStreamer::playWrapper, this);
    Register(METHOD_PAUSE, &GStreamer::pauseWrapper, this);
    Register(METHOD_QUIT, &GStreamer::quitWrapper, this);
}

GStreamer::~GStreamer()
{
    cleanupPipeline();
}

////////////////////////////////////////////////////////////

const string GStreamer::Initialize(PluginHost::IShell* shell)
{
    // Initialize GStreamer
    gst_init(nullptr, nullptr);
    
    // Initialize the pipeline
    initializePipeline();
    
    return {};
}

void GStreamer::Deinitialize(PluginHost::IShell* service)
{
    cleanupPipeline();
}

string GStreamer::Information() const
{
    return "{\"service\": \"" + SERVICE_NAME + "\"}";
}

////////////////////////////////////////////////////////////
//////////////////// INTERNAL LOGIC ////////////////////////
////////////////////////////////////////////////////////////

void GStreamer::initializePipeline()
{
    if (m_pipelineInitialized) {
        return;
    }

    // Create elements
    m_source = gst_element_factory_make("uridecodebin", "source");
    
    // Audio branch
    m_audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
    m_audioresample = gst_element_factory_make("audioresample", "audioresample");
    m_audiosink = gst_element_factory_make("autoaudiosink", "audiosink");
    
    // Video branch
    m_videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    
    // Try platform-specific video sinks with fallback
    // Priority: westerossink (RDK) -> waylandsink -> brcmvideosink -> autovideosink
    m_videosink = gst_element_factory_make("westerossink", "videosink");
    if (!m_videosink) {
        g_print("westerossink not available, trying waylandsink...\n");
        m_videosink = gst_element_factory_make("waylandsink", "videosink");
    }
    if (!m_videosink) {
        g_print("waylandsink not available, trying brcmvideosink...\n");
        m_videosink = gst_element_factory_make("brcmvideosink", "videosink");
    }
    if (!m_videosink) {
        g_print("Platform sinks not available, falling back to autovideosink...\n");
        m_videosink = gst_element_factory_make("autovideosink", "videosink");
    }
    
    // Create pipeline
    m_pipeline = gst_pipeline_new("gstreamer-plugin-pipeline");
    
    if (!m_pipeline || !m_source ||
        !m_audioconvert || !m_audioresample || !m_audiosink ||
        !m_videoconvert || !m_videosink) {
        g_printerr("Not all elements could be created.\n");
        return;
    }
    
    // Build the pipeline
    gst_bin_add_many(GST_BIN(m_pipeline),
                     m_source,
                     m_audioconvert, m_audioresample, m_audiosink,
                     m_videoconvert, m_videosink,
                     NULL);
    
    // Link audio elements
    if (!gst_element_link_many(m_audioconvert,
                               m_audioresample,
                               m_audiosink,
                               NULL)) {
        g_printerr("Audio elements could not be linked.\n");
        cleanupPipeline();
        return;
    }
    
    // Link video elements
    if (!gst_element_link_many(m_videoconvert,
                               m_videosink,
                               NULL)) {
        g_printerr("Video elements could not be linked.\n");
        cleanupPipeline();
        return;
    }
    
    // Set URI
    g_object_set(m_source, "uri",
                 "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
                 NULL);
    
    // Connect to pad-added signal
    g_signal_connect(m_source, "pad-added",
                     G_CALLBACK(padAddedHandler), this);
    
    // Get bus
    m_bus = gst_element_get_bus(m_pipeline);
    
    m_pipelineInitialized = true;
}

void GStreamer::cleanupPipeline()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    
    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }
    
    m_source = nullptr;
    m_audioconvert = nullptr;
    m_audioresample = nullptr;
    m_audiosink = nullptr;
    m_videoconvert = nullptr;
    m_videosink = nullptr;
    m_pipelineInitialized = false;
}

void GStreamer::play()
{
    if (!m_pipelineInitialized || !m_pipeline) {
        g_printerr("Pipeline not initialized.\n");
        return;
    }
    
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
    }
}

void GStreamer::pause()
{
    if (!m_pipelineInitialized || !m_pipeline) {
        g_printerr("Pipeline not initialized.\n");
        return;
    }
    
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the paused state.\n");
    }
}

void GStreamer::quit()
{
    cleanupPipeline();
}

// Static pad-added handler
void GStreamer::padAddedHandler(GstElement* src, GstPad* newPad, GStreamer* data)
{
    GstPad* sinkPad = nullptr;
    GstCaps* newPadCaps = nullptr;
    GstStructure* newPadStruct = nullptr;
    const gchar* newPadType = nullptr;
    GstPadLinkReturn ret;
    
    g_print("Received new pad '%s' from '%s'\n",
            GST_PAD_NAME(newPad),
            GST_ELEMENT_NAME(src));
    
    newPadCaps = gst_pad_get_current_caps(newPad);
    newPadStruct = gst_caps_get_structure(newPadCaps, 0);
    newPadType = gst_structure_get_name(newPadStruct);
    
    // Handle Audio
    if (g_str_has_prefix(newPadType, "audio/x-raw")) {
        sinkPad = gst_element_get_static_pad(data->m_audioconvert, "sink");
        
        if (!gst_pad_is_linked(sinkPad)) {
            ret = gst_pad_link(newPad, sinkPad);
            if (GST_PAD_LINK_FAILED(ret))
                g_print("Audio link failed.\n");
            else
                g_print("Audio link succeeded.\n");
        }
    }
    // Handle Video
    else if (g_str_has_prefix(newPadType, "video/x-raw")) {
        sinkPad = gst_element_get_static_pad(data->m_videoconvert, "sink");
        
        if (!gst_pad_is_linked(sinkPad)) {
            ret = gst_pad_link(newPad, sinkPad);
            if (GST_PAD_LINK_FAILED(ret))
                g_print("Video link failed.\n");
            else
                g_print("Video link succeeded.\n");
        }
    }
    else {
        g_print("Unknown pad type '%s'. Ignoring.\n", newPadType);
    }
    
    if (newPadCaps != nullptr)
        gst_caps_unref(newPadCaps);
    
    if (sinkPad != nullptr)
        gst_object_unref(sinkPad);
}

////////////////////////////////////////////////////////////
//////////////////// WRAPPER METHODS ///////////////////////
////////////////////////////////////////////////////////////

uint32_t GStreamer::playWrapper(const JsonObject& parameters, JsonObject& response)
{
    play();
    response["success"] = true;
    response["message"] = "Pipeline started playing";
    return Core::ERROR_NONE;
}

uint32_t GStreamer::pauseWrapper(const JsonObject& parameters, JsonObject& response)
{
    pause();
    response["success"] = true;
    response["message"] = "Pipeline paused";
    return Core::ERROR_NONE;
}

uint32_t GStreamer::quitWrapper(const JsonObject& parameters, JsonObject& response)
{
    quit();
    response["success"] = true;
    response["message"] = "Pipeline stopped and cleaned up";
    return Core::ERROR_NONE;
}

} // Plugin
} // WPEFramework