#pragma once

#include <QObject>
#include <memory>
#include <atomic>
#include <mutex>
#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include "websocket-client.hpp"
#include "audio-format.hpp"

namespace obs_audio_to_websocket {

class SettingsDialog;

class AudioStreamer : public QObject {
    Q_OBJECT

public:
    static AudioStreamer& Instance();
    
    void Start();
    void Stop();
    bool IsStreaming() const { return m_streaming.load(); }
    
    void SetWebSocketUrl(const std::string& url) { m_wsUrl = url; }
    std::string GetWebSocketUrl() const { return m_wsUrl; }
    
    void SetAudioSource(const std::string& sourceName);
    std::string GetAudioSource() const { return m_audioSourceName; }
    
    void ShowSettings();
    
    double GetDataRate() const { return m_dataRate.load(); }
    bool IsConnected() const { return m_wsClient && m_wsClient->IsConnected(); }

signals:
    void connectionStatusChanged(bool connected);
    void streamingStatusChanged(bool streaming);
    void dataRateChanged(double kbps);
    void errorOccurred(const QString& error);

private:
    AudioStreamer();
    ~AudioStreamer();
    AudioStreamer(const AudioStreamer&) = delete;
    AudioStreamer& operator=(const AudioStreamer&) = delete;
    
    static void AudioCaptureCallback(void* param, obs_source_t* source, 
                                   const struct audio_data* audio_data, bool muted);
    
    void ProcessAudioData(obs_source_t* source, const struct audio_data* audio_data, bool muted);
    void ConnectToWebSocket();
    void DisconnectFromWebSocket();
    void AttachAudioSource();
    void DetachAudioSource();
    
    void OnWebSocketConnected();
    void OnWebSocketDisconnected();
    void OnWebSocketMessage(const std::string& message);
    void OnWebSocketError(const std::string& error);
    
    void UpdateDataRate(size_t bytes);
    
    std::unique_ptr<WebSocketClient> m_wsClient;
    std::unique_ptr<SettingsDialog> m_settingsDialog;
    
    obs_source_t* m_audioSource = nullptr;
    std::string m_audioSourceName;
    std::string m_wsUrl = "ws://localhost:8889/audio";
    
    std::atomic<bool> m_streaming{false};
    std::atomic<double> m_dataRate{0.0};
    
    std::mutex m_sourceMutex;
    
    // Data rate calculation
    std::chrono::steady_clock::time_point m_lastRateUpdate;
    size_t m_bytesSinceLastUpdate = 0;
    std::mutex m_rateMutex;
};

} // namespace obs_audio_to_websocket

// C-style interface for OBS
extern "C" {
    EXPORT void obs_module_load(void);
    EXPORT void obs_module_unload(void);
    EXPORT const char* obs_module_name(void);
    EXPORT const char* obs_module_description(void);
}