#pragma once

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include "audio-format.hpp"

namespace obs_audio_to_websocket {

class WebSocketClient {
public:
    using Client = websocketpp::client<websocketpp::config::asio_client>;
    using MessagePtr = websocketpp::config::asio_client::message_type::ptr;
    using ConnectionHdl = websocketpp::connection_hdl;
    
    using OnConnectedCallback = std::function<void()>;
    using OnDisconnectedCallback = std::function<void()>;
    using OnMessageCallback = std::function<void(const std::string&)>;
    using OnErrorCallback = std::function<void(const std::string&)>;

    WebSocketClient();
    ~WebSocketClient();

    bool Connect(const std::string& uri);
    void Disconnect();
    bool IsConnected() const { return m_connected.load(); }
    
    void SendAudioData(const AudioChunk& chunk);
    void SendControlMessage(const std::string& type);
    
    void SetOnConnected(OnConnectedCallback cb) { m_onConnected = cb; }
    void SetOnDisconnected(OnDisconnectedCallback cb) { m_onDisconnected = cb; }
    void SetOnMessage(OnMessageCallback cb) { m_onMessage = cb; }
    void SetOnError(OnErrorCallback cb) { m_onError = cb; }

private:
    void Run();
    void OnOpen(ConnectionHdl hdl);
    void OnClose(ConnectionHdl hdl);
    void OnMessage(ConnectionHdl hdl, MessagePtr msg);
    void OnFail(ConnectionHdl hdl);
    
    void ProcessSendQueue();
    void StartReconnectTimer();
    void StopReconnectTimer();
    
    Client m_client;
    std::thread m_thread;
    std::thread m_sendThread;
    std::thread m_reconnectThread;
    
    ConnectionHdl m_hdl;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_reconnecting{false};
    
    std::string m_uri;
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendMutex;
    std::condition_variable m_sendCv;
    
    OnConnectedCallback m_onConnected;
    OnDisconnectedCallback m_onDisconnected;
    OnMessageCallback m_onMessage;
    OnErrorCallback m_onError;
    
    int m_reconnectDelay = 1000; // Start with 1 second
    const int m_maxReconnectDelay = 30000; // Max 30 seconds
};

} // namespace obs_audio_to_websocket