#include "obs-audio-to-websocket/audio-streamer.hpp"
#include "obs-audio-to-websocket/settings-dialog.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <util/platform.h>
#include <util/threading.h>
#include <util/config-file.h>
#include <obs-module.h>

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(param) (void)param
#endif

namespace obs_audio_to_websocket {

AudioStreamer &AudioStreamer::Instance()
{
	static AudioStreamer instance;
	return instance;
}

AudioStreamer::AudioStreamer() : m_lastRateUpdate(std::chrono::steady_clock::now()) {}

AudioStreamer::~AudioStreamer()
{
	m_shuttingDown = true;
	Stop();
}

void AudioStreamer::Start()
{
	if (m_streaming)
		return;

	m_streaming = true;

	ConnectToWebSocket();
	AttachAudioSource();

	emit streamingStatusChanged(true);
}

void AudioStreamer::Stop()
{
	if (!m_streaming)
		return;

	m_streaming = false;

	DetachAudioSource();
	DisconnectFromWebSocket();

	emit streamingStatusChanged(false);
}

void AudioStreamer::SetAudioSource(const std::string &sourceName)
{
	std::lock_guard<std::recursive_mutex> lock(m_sourceMutex);

	if (m_audioSourceName == sourceName)
		return;

	bool wasStreaming = m_streaming.load();
	if (wasStreaming) {
		DetachAudioSource();
	}

	m_audioSourceName = sourceName;

	if (wasStreaming) {
		AttachAudioSource();
	}
}

void AudioStreamer::ShowSettings()
{
	if (!m_settingsDialog) {
		m_settingsDialog = std::make_unique<SettingsDialog>();
	}
	m_settingsDialog->show();
	m_settingsDialog->raise();
	m_settingsDialog->activateWindow();
}

void AudioStreamer::LoadSettings()
{
	// Load from OBS user config
#if LIBOBS_API_MAJOR_VER >= 31
	config_t *config = obs_frontend_get_user_config();
#else
	config_t *config = obs_frontend_get_profile_config();
#endif

	const char *url = config_get_string(config, "AudioStreamer", "WebSocketUrl");
	if (url && strlen(url) > 0) {
		std::lock_guard<std::mutex> lock(m_urlMutex);
		m_wsUrl = url;
	}

	const char *source = config_get_string(config, "AudioStreamer", "AudioSource");
	if (source && strlen(source) > 0) {
		std::lock_guard<std::recursive_mutex> lock(m_sourceMutex);
		m_audioSourceName = source;
	}
}

void AudioStreamer::ConnectToWebSocket()
{
	if (!m_wsClient) {
		m_wsClient = std::make_shared<WebSocketPPClient>();

		m_wsClient->SetOnConnected([this]() { OnWebSocketConnected(); });
		m_wsClient->SetOnDisconnected([this]() { OnWebSocketDisconnected(); });
		m_wsClient->SetOnMessage([this](const std::string &msg) { OnWebSocketMessage(msg); });
		m_wsClient->SetOnError([this](const std::string &err) { OnWebSocketError(err); });
	}

	std::string url;
	{
		std::lock_guard<std::mutex> lock(m_urlMutex);
		url = m_wsUrl;
	}
	m_wsClient->Connect(url);
}

void AudioStreamer::DisconnectFromWebSocket()
{
	if (m_wsClient) {
		m_wsClient->SendControlMessage("stop");
		m_wsClient->Disconnect();
	}
}

void AudioStreamer::AttachAudioSource()
{
	std::lock_guard<std::recursive_mutex> lock(m_sourceMutex);

	if (m_audioSourceName.empty()) {
		blog(LOG_WARNING, "[Audio to WebSocket] No audio source name specified");
		return;
	}

	// Check if already attached to the same source
	if (m_audioSource) {
		const char *current_name = obs_source_get_name(m_audioSource);
		if (current_name && m_audioSourceName == current_name) {
			return;
		}
	}

	// Detach any existing source first
	DetachAudioSource();

	// Get the source by name
	obs_source_t *source = obs_get_source_by_name(m_audioSourceName.c_str());
	if (!source) {
		blog(LOG_ERROR, "[Audio to WebSocket] Audio source '%s' not found", m_audioSourceName.c_str());
		emit errorOccurred(
			QString("Audio source '%1' not found").arg(QString::fromStdString(m_audioSourceName)));
		// Stop streaming if source attachment fails
		if (m_streaming) {
			Stop();
		}
		return;
	}

	// Verify it's an audio source
	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_AUDIO)) {
		blog(LOG_ERROR, "[Audio to WebSocket] Source '%s' is not an audio source", m_audioSourceName.c_str());
		emit errorOccurred(
			QString("Source '%1' is not an audio source").arg(QString::fromStdString(m_audioSourceName)));
		obs_source_release(source);
		// Stop streaming if source attachment fails
		if (m_streaming) {
			Stop();
		}
		return;
	}

	m_audioSource = source;

	// Use OBS audio capture API with static callback
	obs_source_add_audio_capture_callback(m_audioSource, AudioCaptureCallback, this);
}

void AudioStreamer::DetachAudioSource()
{
	std::lock_guard<std::recursive_mutex> lock(m_sourceMutex);

	if (m_audioSource) {
		// Remove audio capture callback
		obs_source_remove_audio_capture_callback(m_audioSource, AudioCaptureCallback, this);

		obs_source_release(m_audioSource);
		m_audioSource = nullptr;
	}
}

void AudioStreamer::AudioCaptureCallback(void *param, obs_source_t *source, const struct audio_data *audio_data,
					 bool muted)
{
	auto *streamer = static_cast<AudioStreamer *>(param);
	streamer->ProcessAudioData(source, audio_data, muted);
}

void AudioStreamer::ProcessAudioData(obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
	static bool first_call = true;
	static int callback_count = 0;
	callback_count++;

	first_call = false;

	bool is_connected = m_wsClient && m_wsClient->IsConnected();

	if (m_shuttingDown || !m_streaming || muted || !m_wsClient || !is_connected) {
		return;
	}

	const audio_output_info *aoi = audio_output_get_info(obs_get_audio());
	if (!aoi)
		return;

	uint32_t sample_rate = aoi->samples_per_sec;
	uint32_t channels = static_cast<uint32_t>(audio_output_get_channels(obs_get_audio()));

	// Convert to 16-bit PCM if needed
	size_t frames = audio_data->frames;
	size_t sample_size = sizeof(int16_t);
	size_t data_size = frames * channels * sample_size;

	AudioChunk chunk;
	chunk.data.resize(data_size);
	chunk.timestamp = audio_data->timestamp;
	chunk.format = AudioFormat(sample_rate, channels, 16);
	chunk.sourceId = obs_source_get_name(source);
	chunk.sourceName = obs_source_get_name(source);

	// Copy and convert audio data to 16-bit signed PCM (little-endian)
	int16_t *out_ptr = reinterpret_cast<int16_t *>(chunk.data.data());

	// Variables for audio level analysis
	float peak_level = 0.0f;
	float rms_sum = 0.0f;
	size_t total_samples = 0;

	for (size_t ch = 0; ch < channels; ++ch) {
		const float *in_ptr = reinterpret_cast<const float *>(audio_data->data[ch]);
		for (size_t i = 0; i < frames; ++i) {
			float sample = in_ptr[i];

			// Track audio levels
			float abs_sample = std::abs(sample);
			if (abs_sample > peak_level) {
				peak_level = abs_sample;
			}
			rms_sum += sample * sample;
			total_samples++;

			// Clamp to [-1, 1] range
			sample = (std::max)(-1.0f, (std::min)(1.0f, sample));
			// Convert to 16-bit signed PCM with proper rounding
			// Note: This produces little-endian output on x86/x64 systems
			out_ptr[i * channels + ch] = static_cast<int16_t>(std::round(sample * 32767.0f));
		}
	}

	// Calculate RMS level
	float rms_level = 0.0f;
	if (total_samples > 0) {
		rms_level = std::sqrt(rms_sum / total_samples);
	}

	// Log audio format info only once at startup
	static bool format_logged = false;
	if (!format_logged) {
		format_logged = true;
		blog(LOG_INFO, "[Audio to WebSocket] Streaming %u Hz, %u ch, 16-bit PCM (LE)", sample_rate, channels);
	}

	// Only warn about silence, don't log normal levels
	static int silence_counter = 0;
	if (peak_level < 0.0001f) { // Essentially silence
		silence_counter++;
		if (silence_counter == 500) { // After ~10 seconds of silence
			blog(LOG_WARNING, "[Audio to WebSocket] No audio detected - check source");
		}
	} else {
		silence_counter = 0;
	}

	m_wsClient->SendAudioData(chunk);
	UpdateDataRate(data_size);
}

void AudioStreamer::OnWebSocketConnected()
{
	emit connectionStatusChanged(true);
}

void AudioStreamer::OnWebSocketDisconnected()
{
	emit connectionStatusChanged(false);
}

void AudioStreamer::OnWebSocketMessage(const std::string &message)
{
	UNUSED_PARAMETER(message);
	// Handle status messages from server
	try {
		// Parse JSON status message if needed
	} catch (...) {
		// Ignore parse errors
	}
}

void AudioStreamer::OnWebSocketError(const std::string &error)
{
	emit errorOccurred(QString::fromStdString(error));
}

void AudioStreamer::UpdateDataRate(size_t bytes)
{
	std::lock_guard<std::mutex> lock(m_rateMutex);

	m_bytesSinceLastUpdate += bytes;

	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRateUpdate);

	if (elapsed.count() >= 1000) {                                          // Update every second
		double kbps = (m_bytesSinceLastUpdate * 8.0) / elapsed.count(); // Convert to kilobits per second
		m_dataRate = kbps;

		m_bytesSinceLastUpdate = 0;
		m_lastRateUpdate = now;

		emit dataRateChanged(kbps);
	}
}

} // namespace obs_audio_to_websocket
