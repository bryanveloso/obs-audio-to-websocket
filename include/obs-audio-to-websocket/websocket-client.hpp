#pragma once

#include <libwebsockets.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include "audio-format.hpp"

namespace obs_audio_to_websocket {

class WebSocketClient {
public:
	using OnConnectedCallback = std::function<void()>;
	using OnDisconnectedCallback = std::function<void()>;
	using OnMessageCallback = std::function<void(const std::string &)>;
	using OnErrorCallback = std::function<void(const std::string &)>;

	WebSocketClient();
	~WebSocketClient();

	bool Connect(const std::string &uri);
	void Disconnect();
	bool IsConnected() const { return m_connected.load(); }

	void SendAudioData(const AudioChunk &chunk);
	void SendControlMessage(const std::string &type);

	void SetOnConnected(OnConnectedCallback cb) { m_onConnected = cb; }
	void SetOnDisconnected(OnDisconnectedCallback cb) { m_onDisconnected = cb; }
	void SetOnMessage(OnMessageCallback cb) { m_onMessage = cb; }
	void SetOnError(OnErrorCallback cb) { m_onError = cb; }

	static int LwsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

private:

	void Run();
	void ProcessSendQueue();
	void StartReconnectTimer();
	void StopReconnectTimer();
	void SendQueuedMessage(const std::string &message);

	struct SendBuffer {
		std::vector<uint8_t> data;
		size_t sent = 0;
	};

	struct lws_context *m_context = nullptr;
	struct lws *m_wsi = nullptr;
	std::thread m_thread;
	std::thread m_reconnectThread;

	std::atomic<bool> m_connected{false};
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_reconnecting{false};
	std::atomic<bool> m_shouldReconnect{true};

	std::string m_uri;
	std::string m_host;
	std::string m_path;
	int m_port = 80;

	std::queue<std::string> m_sendQueue;
	std::mutex m_sendMutex;
	std::condition_variable m_sendCv;

	std::unique_ptr<SendBuffer> m_currentSendBuffer;
	std::mutex m_writeMutex;

	OnConnectedCallback m_onConnected;
	OnDisconnectedCallback m_onDisconnected;
	OnMessageCallback m_onMessage;
	OnErrorCallback m_onError;

	int m_reconnectDelay = 1000;           // Start with 1 second
	const int m_maxReconnectDelay = 30000; // Max 30 seconds

	// Static instance map for callback routing
	static std::map<struct lws *, WebSocketClient *> s_instances;
	static std::mutex s_instancesMutex;
};

} // namespace obs_audio_to_websocket
