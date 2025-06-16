#include "obs-audio-to-websocket/websocket-client.hpp"
#include <nlohmann/json.hpp>
#include <chrono>

namespace obs_audio_to_websocket {

using json = nlohmann::json;

WebSocketClient::WebSocketClient() {
    m_client.clear_access_channels(websocketpp::log::alevel::all);
    m_client.clear_error_channels(websocketpp::log::elevel::all);
    
    m_client.init_asio();
    
    m_client.set_open_handler([this](ConnectionHdl hdl) { OnOpen(hdl); });
    m_client.set_close_handler([this](ConnectionHdl hdl) { OnClose(hdl); });
    m_client.set_message_handler([this](ConnectionHdl hdl, MessagePtr msg) { OnMessage(hdl, msg); });
    m_client.set_fail_handler([this](ConnectionHdl hdl) { OnFail(hdl); });
}

WebSocketClient::~WebSocketClient() {
    Disconnect();
}

bool WebSocketClient::Connect(const std::string& uri) {
    if (m_connected || m_running) {
        return false;
    }
    
    m_uri = uri;
    m_running = true;
    
    try {
        websocketpp::lib::error_code ec;
        Client::connection_ptr con = m_client.get_connection(uri, ec);
        
        if (ec) {
            if (m_onError) {
                m_onError("Connection initialization failed: " + ec.message());
            }
            m_running = false;
            return false;
        }
        
        m_client.connect(con);
        
        m_thread = std::thread(&WebSocketClient::Run, this);
        m_sendThread = std::thread(&WebSocketClient::ProcessSendQueue, this);
        
        return true;
    } catch (const std::exception& e) {
        if (m_onError) {
            m_onError("Connection failed: " + std::string(e.what()));
        }
        m_running = false;
        return false;
    }
}

void WebSocketClient::Disconnect() {
    StopReconnectTimer();
    m_running = false;
    
    if (m_connected) {
        websocketpp::lib::error_code ec;
        m_client.close(m_hdl, websocketpp::close::status::going_away, "Client disconnect", ec);
    }
    
    m_client.stop();
    
    m_sendCv.notify_all();
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }
    
    m_connected = false;
}

void WebSocketClient::Run() {
    try {
        m_client.run();
    } catch (const std::exception& e) {
        if (m_onError) {
            m_onError("WebSocket runtime error: " + std::string(e.what()));
        }
    }
}

void WebSocketClient::OnOpen(ConnectionHdl hdl) {
    m_hdl = hdl;
    m_connected = true;
    m_reconnectDelay = 1000; // Reset reconnect delay
    
    if (m_onConnected) {
        m_onConnected();
    }
    
    SendControlMessage("start");
}

void WebSocketClient::OnClose(ConnectionHdl hdl) {
    m_connected = false;
    
    if (m_onDisconnected) {
        m_onDisconnected();
    }
    
    if (m_running && !m_reconnecting) {
        StartReconnectTimer();
    }
}

void WebSocketClient::OnMessage(ConnectionHdl hdl, MessagePtr msg) {
    if (m_onMessage) {
        m_onMessage(msg->get_payload());
    }
}

void WebSocketClient::OnFail(ConnectionHdl hdl) {
    m_connected = false;
    
    if (m_onError) {
        m_onError("Connection failed");
    }
    
    if (m_running && !m_reconnecting) {
        StartReconnectTimer();
    }
}

void WebSocketClient::SendAudioData(const AudioChunk& chunk) {
    if (!m_connected) return;
    
    json msg;
    msg["type"] = "audio_data";
    msg["timestamp"] = chunk.timestamp;
    msg["format"]["sampleRate"] = chunk.format.sampleRate;
    msg["format"]["channels"] = chunk.format.channels;
    msg["format"]["bitDepth"] = chunk.format.bitDepth;
    msg["data"] = base64_encode(chunk.data.data(), chunk.data.size());
    msg["sourceId"] = chunk.sourceId;
    msg["sourceName"] = chunk.sourceName;
    
    std::string payload = msg.dump();
    
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.push(payload);
    }
    m_sendCv.notify_one();
}

void WebSocketClient::SendControlMessage(const std::string& type) {
    if (!m_connected) return;
    
    json msg;
    msg["type"] = type;
    msg["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::string payload = msg.dump();
    
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.push(payload);
    }
    m_sendCv.notify_one();
}

void WebSocketClient::ProcessSendQueue() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_sendMutex);
        m_sendCv.wait(lock, [this] { return !m_sendQueue.empty() || !m_running; });
        
        while (!m_sendQueue.empty() && m_connected) {
            std::string msg = m_sendQueue.front();
            m_sendQueue.pop();
            lock.unlock();
            
            try {
                websocketpp::lib::error_code ec;
                m_client.send(m_hdl, msg, websocketpp::frame::opcode::text, ec);
                
                if (ec && m_onError) {
                    m_onError("Send failed: " + ec.message());
                }
            } catch (const std::exception& e) {
                if (m_onError) {
                    m_onError("Send exception: " + std::string(e.what()));
                }
            }
            
            lock.lock();
        }
    }
}

void WebSocketClient::StartReconnectTimer() {
    if (m_reconnecting.exchange(true)) {
        return; // Already reconnecting
    }
    
    m_reconnectThread = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_reconnectDelay));
        
        if (!m_running || m_connected) {
            m_reconnecting = false;
            return;
        }
        
        // Exponential backoff
        m_reconnectDelay = std::min(m_reconnectDelay * 2, m_maxReconnectDelay);
        
        m_client.reset();
        m_connected = false;
        m_running = false;
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
        
        m_running = true;
        
        if (Connect(m_uri)) {
            if (m_onError) {
                m_onError("Reconnecting...");
            }
        }
        
        m_reconnecting = false;
    });
    
    m_reconnectThread.detach();
}

void WebSocketClient::StopReconnectTimer() {
    m_reconnecting = false;
}

} // namespace obs_audio_to_websocket