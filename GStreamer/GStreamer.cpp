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
const string GStreamer::METHOD_INITIALIZE = "initialize";
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
    Register(METHOD_INITIALIZE, &GStreamer::initializeWrapper, this);
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
    // Initialize GStreamer library only
    gst_init(nullptr, nullptr);
    
    // Pipeline will be built when user calls initialize() JSON-RPC method
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

string GStreamer::buildPipeline()
{
    if (m_pipelineInitialized) {
        return string("Pipeline already initialized");
    }

    // Create elements
    m_source = gst_element_factory_make("uridecodebin", "source");
    if (!m_source) return string("Failed to create uridecodebin element");
    
    // Audio branch
    m_audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
    if (!m_audioconvert) return string("Failed to create audioconvert element");
    
    m_audioresample = gst_element_factory_make("audioresample", "audioresample");
    if (!m_audioresample) return string("Failed to create audioresample element");
    
    m_audiosink = gst_element_factory_make("autoaudiosink", "audiosink");
    if (!m_audiosink) return string("Failed to create autoaudiosink");

    // Video branch
    m_videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    if (!m_videoconvert) return string("Failed to create videoconvert element");
    
    m_videosink = gst_element_factory_make("westerossink", "videosink");
    if (!m_videosink) return string("Failed to create westerossink");
    
    // Create pipeline
    m_pipeline = gst_pipeline_new("gstreamer-plugin-pipeline");
    if (!m_pipeline) return string("Failed to create GStreamer pipeline");
    
    // Build the pipeline - add all elements
    gst_bin_add_many(GST_BIN(m_pipeline),
                     m_source,
                     m_audioconvert, m_audioresample, m_audiosink,
                     m_videoconvert, m_videosink,
                     NULL);
    
    // Link audio elements
    if (!gst_element_link_many(m_audioconvert, m_audioresample, m_audiosink, NULL)) {
        cleanupPipeline();
        return string("Failed to link audio pipeline elements");
    }
    
    // Link video elements
    if (!gst_element_link_many(m_videoconvert, m_videosink, NULL)) {
        cleanupPipeline();
        return string("Failed to link video pipeline elements");
    }
    
    // Set URI
    g_object_set(m_source, "uri",
                 "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
                 NULL);
    
    // Connect to pad-added signal for dynamic linking
    g_signal_connect(m_source, "pad-added", G_CALLBACK(padAddedHandler), this);
    
    // Get bus
    m_bus = gst_element_get_bus(m_pipeline);
    
    // Set to READY state (pipeline built but not playing)
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_READY);

    if (ret == GST_STATE_CHANGE_FAILURE) {

        GstMessage *msg = gst_bus_pop_filtered(
            m_bus,
            GST_MESSAGE_ERROR
        );

        if (msg) {
            GError *err = NULL;
            gchar *debug = NULL;

            gst_message_parse_error(msg, &err, &debug);

            std::string errorMsg = err->message;

            g_error_free(err);
            g_free(debug);
            gst_message_unref(msg);

            cleanupPipeline();
            return errorMsg;
        }

        cleanupPipeline();
        return string("Failed to set pipeline to READY state");
    }
    
    m_pipelineInitialized = true;
    return string();  // Empty = success
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

string GStreamer::playPipeline()
{
    if (!m_pipelineInitialized || !m_pipeline) {
        return string("Pipeline not initialized. Call initialize() first.");
    }
    
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return string("Failed to set pipeline to PLAYING state");
    }
    
    return string();  // Success
}

string GStreamer::pausePipeline()
{
    if (!m_pipelineInitialized || !m_pipeline) {
        return string("Pipeline not initialized");
    }
    
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return string("Failed to set pipeline to PAUSED state");
    }
    
    return string("success");  // Success
}

// Static pad-added handler
void GStreamer::padAddedHandler(GstElement* src, GstPad* newPad, GStreamer* data)
{
    GstPad* sinkPad = nullptr;
    GstCaps* newPadCaps = nullptr;
    GstStructure* newPadStruct = nullptr;
    const gchar* newPadType = nullptr;
    GstPadLinkReturn ret;
    
    //---------------Need to edit this--------------------------//
    //---------------remove gprint command as they might not be useful or ot may not print in the console
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

uint32_t GStreamer::initializeWrapper(const JsonObject& parameters, JsonObject& response)
{
    string error = buildPipeline();
    
    if (!error.empty()) {
        response["success"] = false;
        response["message"] = error;
        return Core::ERROR_NONE;
    }
    
    response["success"] = true;
    response["message"] = "Pipeline initialized successfully. Ready to play.";
    return Core::ERROR_NONE;
}

uint32_t GStreamer::playWrapper(const JsonObject& parameters, JsonObject& response)
{
    string error = playPipeline();
    
    if (!error.empty()) {
        response["success"] = false;
        response["message"] = error;
        return Core::ERROR_NONE;
    }
    
    response["success"] = true;
    response["message"] = "Video playback started";
    return Core::ERROR_NONE;
}

uint32_t GStreamer::pauseWrapper(const JsonObject& parameters, JsonObject& response)
{
    string error = pausePipeline();
    
    if (!error.empty()) {
        response["success"] = false;
        response["message"] = error;
        return Core::ERROR_NONE;
    }
    
    response["success"] = true;
    response["message"] = "Video playback paused";
    return Core::ERROR_NONE;
}

uint32_t GStreamer::quitWrapper(const JsonObject& parameters, JsonObject& response)
{
    if (!m_pipelineInitialized) {
        response["success"] = false;
        response["message"] = "No active pipeline to quit";
        return Core::ERROR_NONE;
    }
    
    cleanupPipeline();
    response["success"] = true;
    response["message"] = "Pipeline stopped and cleaned up";
    return Core::ERROR_NONE;
}

} // Plugin
} // WPEFramework