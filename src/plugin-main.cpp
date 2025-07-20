#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include "obs-audio-to-websocket/audio-streamer.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-audio-to-websocket", "en-US")

void on_frontend_event(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		if (obs_audio_to_websocket::AudioStreamer::Instance().IsAutoConnectEnabled()) {
			blog(LOG_INFO, "[Audio to WebSocket] Auto-connect enabled: Starting audio streaming");
			obs_audio_to_websocket::AudioStreamer::Instance().Start();
		}
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		if (obs_audio_to_websocket::AudioStreamer::Instance().IsAutoConnectEnabled() &&
		    obs_audio_to_websocket::AudioStreamer::Instance().IsStreaming()) {
			blog(LOG_INFO, "[Audio to WebSocket] Auto-connect enabled: Stopping audio streaming");
			obs_audio_to_websocket::AudioStreamer::Instance().Stop();
		}
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		obs_audio_to_websocket::AudioStreamer::Instance().Stop();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	// Get main window
	QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window)
		return false;

	// Load saved settings
	obs_audio_to_websocket::AudioStreamer::Instance().LoadSettings();

	obs_frontend_add_tools_menu_item(
		obs_module_text("AudioStreamerSettings"),
		[](void *data) {
			UNUSED_PARAMETER(data);
			obs_audio_to_websocket::AudioStreamer::Instance().ShowSettings();
		},
		nullptr);

	// Register for frontend events
	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	blog(LOG_INFO, "[Audio to WebSocket] Plugin loaded successfully");
	return true;
}

void obs_module_unload(void)
{
	obs_audio_to_websocket::AudioStreamer::Instance().Stop();
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);

	blog(LOG_INFO, "[Audio to WebSocket] Plugin unloaded");
}

const char *obs_module_name(void)
{
	return "Audio to WebSocket";
}

const char *obs_module_description(void)
{
	return "Stream audio from OBS sources to WebSocket endpoints for remote processing";
}
