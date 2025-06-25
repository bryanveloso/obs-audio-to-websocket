#include "obs-audio-to-websocket/audio-streamer.hpp"
#include "obs-audio-to-websocket/settings-dialog.hpp"
#include <chrono>
#include <util/platform.h>
#include <util/threading.h>
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
	std::lock_guard<std::mutex> lock(m_sourceMutex);

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

void AudioStreamer::ConnectToWebSocket()
{
	if (!m_wsClient) {
		m_wsClient = std::make_unique<WebSocketClient>();

		m_wsClient->SetOnConnected([this]() { OnWebSocketConnected(); });
		m_wsClient->SetOnDisconnected([this]() { OnWebSocketDisconnected(); });
		m_wsClient->SetOnMessage([this](const std::string &msg) { OnWebSocketMessage(msg); });
		m_wsClient->SetOnError([this](const std::string &err) { OnWebSocketError(err); });
	}

	m_wsClient->Connect(m_wsUrl);
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
	std::lock_guard<std::mutex> lock(m_sourceMutex);

	if (m_audioSourceName.empty())
		return;

	DetachAudioSource();

	m_audioSource = obs_get_source_by_name(m_audioSourceName.c_str());
	if (!m_audioSource) {
		emit errorOccurred(
			QString("Audio source '%1' not found").arg(QString::fromStdString(m_audioSourceName)));
		return;
	}

	// Use OBS audio capture API
	obs_source_add_audio_capture_callback(
		m_audioSource,
		[](void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted) {
			auto *streamer = static_cast<AudioStreamer *>(param);
			streamer->ProcessAudioData(source, audio_data, muted);
		},
		this);
}

void AudioStreamer::DetachAudioSource()
{
	std::lock_guard<std::mutex> lock(m_sourceMutex);

	if (m_audioSource) {
		// Remove audio capture callback
		obs_source_remove_audio_capture_callback(
			m_audioSource,
			[](void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted) {
				auto *streamer = static_cast<AudioStreamer *>(param);
				streamer->ProcessAudioData(source, audio_data, muted);
			},
			this);

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
	if (!m_streaming || muted || !m_wsClient || !m_wsClient->IsConnected()) {
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

	// Copy and convert audio data to 16-bit PCM
	int16_t *out_ptr = reinterpret_cast<int16_t *>(chunk.data.data());

	for (size_t ch = 0; ch < channels; ++ch) {
		const float *in_ptr = reinterpret_cast<const float *>(audio_data->data[ch]);
		for (size_t i = 0; i < frames; ++i) {
			float sample = in_ptr[i];
			// Clamp to [-1, 1] range
			sample = (std::max)(-1.0f, (std::min)(1.0f, sample));
			// Convert to 16-bit
			out_ptr[i * channels + ch] = static_cast<int16_t>(sample * 32767.0f);
		}
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
