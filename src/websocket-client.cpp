#include "obs-audio-to-websocket/websocket-client.hpp"
#include <obs-module.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <sstream>

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(param) (void)param
#endif

namespace obs_audio_to_websocket {

using json = nlohmann::json;

// Static members
std::map<struct lws*, WebSocketClient*> WebSocketClient::s_instances;
std::mutex WebSocketClient::s_instancesMutex;

// LWS protocols
static struct lws_protocols protocols[] = {
    {
        "audio-protocol",
        WebSocketClient::LwsCallback,
        0,
        65536, // rx buffer size
        0,
        NULL,
        0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

WebSocketClient::WebSocketClient() {
    lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);
}

WebSocketClient::~WebSocketClient() {
    Disconnect();
}

bool WebSocketClient::Connect(const std::string& uri) {
    if (m_connected || m_running) {
        return false;
    }
    
    m_uri = uri;
    m_shouldReconnect = true;
    
    // Parse URI
    std::string protocol, host, path;
    int port = 80;
    
    // Simple URI parser
    size_t pos = uri.find("://");
    if (pos != std::string::npos) {
        protocol = uri.substr(0, pos);
        size_t hostStart = pos + 3;
        size_t pathStart = uri.find('/', hostStart);
        
        if (pathStart != std::string::npos) {
            std::string hostPort = uri.substr(hostStart, pathStart - hostStart);
            path = uri.substr(pathStart);
            
            size_t colonPos = hostPort.find(':');
            if (colonPos != std::string::npos) {
                host = hostPort.substr(0, colonPos);
                port = std::stoi(hostPort.substr(colonPos + 1));
            } else {
                host = hostPort;
            }
        } else {
            host = uri.substr(hostStart);
            path = "/";
        }
    }
    
    if (protocol != "ws" && protocol != "wss") {
        if (m_onError) {
            m_onError("Invalid protocol: " + protocol);
        }
        return false;
    }
    
    m_host = host;
    m_path = path;
    m_port = port;
    
    // Create LWS context
    struct lws_context_creation_info info = {};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    
    m_context = lws_create_context(&info);
    if (!m_context) {
        if (m_onError) {
            m_onError("Failed to create LWS context");
        }
        return false;
    }
    
    m_running = true;
    
    // Start event loop thread
    m_thread = std::thread(&WebSocketClient::Run, this);
    
    // Create connection
    struct lws_client_connect_info connectInfo = {};
    connectInfo.context = m_context;
    connectInfo.address = m_host.c_str();
    connectInfo.port = m_port;
    connectInfo.path = m_path.c_str();
    connectInfo.host = connectInfo.address;
    connectInfo.origin = connectInfo.address;
    connectInfo.ssl_connection = (protocol == "wss") ? LCCSCF_USE_SSL : 0;
    connectInfo.protocol = protocols[0].name;
    connectInfo.userdata = this;
    
    {
        std::lock_guard<std::mutex> lock(s_instancesMutex);
        m_wsi = lws_client_connect_via_info(&connectInfo);
        if (m_wsi) {
            s_instances[m_wsi] = this;
        }
    }
    
    if (!m_wsi) {
        if (m_onError) {
            m_onError("Failed to create connection");
        }
        m_running = false;
        lws_context_destroy(m_context);
        m_context = nullptr;
        return false;
    }
    
    return true;
}

void WebSocketClient::Disconnect() {
    StopReconnectTimer();
    m_running = false;
    m_shouldReconnect = false;
    
    if (m_context) {
        lws_cancel_service(m_context);
    }
    
    m_sendCv.notify_all();
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
    
    if (m_context) {
        lws_context_destroy(m_context);
        m_context = nullptr;
    }
    
    m_connected = false;
    m_wsi = nullptr;
}

void WebSocketClient::Run() {
    while (m_running && m_context) {
        lws_service(m_context, 50);
        ProcessSendQueue();
    }
}

void WebSocketClient::ProcessSendQueue() {
    if (!m_connected || !m_wsi) {
        return;
    }
    
    std::unique_lock<std::mutex> lock(m_sendMutex);
    if (!m_sendQueue.empty() && !m_currentSendBuffer) {
        std::string message = m_sendQueue.front();
        m_sendQueue.pop();
        lock.unlock();
        
        SendQueuedMessage(message);
    }
}

void WebSocketClient::SendQueuedMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    
    if (!m_connected || !m_wsi) {
        return;
    }
    
    // Prepare buffer with LWS header space
    size_t msgLen = message.length();
    m_currentSendBuffer = std::make_unique<SendBuffer>();
    m_currentSendBuffer->data.resize(LWS_PRE + msgLen);
    
    // Copy message after LWS_PRE bytes
    memcpy(m_currentSendBuffer->data.data() + LWS_PRE, message.c_str(), msgLen);
    m_currentSendBuffer->sent = 0;
    
    // Request writeable callback
    lws_callback_on_writable(m_wsi);
}

void WebSocketClient::SendAudioData(const AudioChunk& chunk) {
    if (!m_connected) return;
    
    // Convert to binary format for efficiency
    // Create a simple binary protocol: [format_header][audio_data]
    std::vector<uint8_t> binaryData;
    
    // Header: timestamp(8) + sampleRate(4) + channels(4) + bitDepth(4) + sourceIdLen(4) + sourceNameLen(4)
    size_t headerSize = 8 + 4 + 4 + 4 + 4 + 4;
    size_t sourceIdLen = chunk.sourceId.length();
    size_t sourceNameLen = chunk.sourceName.length();
    size_t totalSize = headerSize + sourceIdLen + sourceNameLen + chunk.data.size();
    
    binaryData.reserve(totalSize);
    
    // Write header
    auto writeUint64 = [&](uint64_t val) {
        for (int i = 0; i < 8; ++i) {
            binaryData.push_back((val >> (i * 8)) & 0xFF);
        }
    };
    
    auto writeUint32 = [&](uint32_t val) {
        for (int i = 0; i < 4; ++i) {
            binaryData.push_back((val >> (i * 8)) & 0xFF);
        }
    };
    
    writeUint64(chunk.timestamp);
    writeUint32(chunk.format.sampleRate);
    writeUint32(chunk.format.channels);
    writeUint32(chunk.format.bitDepth);
    writeUint32(static_cast<uint32_t>(sourceIdLen));
    writeUint32(static_cast<uint32_t>(sourceNameLen));
    
    // Write strings
    binaryData.insert(binaryData.end(), chunk.sourceId.begin(), chunk.sourceId.end());
    binaryData.insert(binaryData.end(), chunk.sourceName.begin(), chunk.sourceName.end());
    
    // Write audio data
    binaryData.insert(binaryData.end(), chunk.data.begin(), chunk.data.end());
    
    // Send as binary message
    std::string binaryMessage(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
    
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendQueue.push(binaryMessage);
    }
    
    // Request write callback
    if (m_wsi) {
        lws_callback_on_writable(m_wsi);
    }
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
    
    // Request write callback
    if (m_wsi) {
        lws_callback_on_writable(m_wsi);
    }
}

int WebSocketClient::LwsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    UNUSED_PARAMETER(user);
    
    WebSocketClient* client = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(s_instancesMutex);
        auto it = s_instances.find(wsi);
        if (it != s_instances.end()) {
            client = it->second;
        }
    }
    
    if (!client) {
        return 0;
    }
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            blog(LOG_INFO, "WebSocket connection established");
            client->m_connected = true;
            client->m_reconnectDelay = 1000; // Reset reconnect delay
            
            if (client->m_onConnected) {
                client->m_onConnected();
            }
            
            client->SendControlMessage("start");
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            blog(LOG_ERROR, "WebSocket connection error: %s", in ? (char*)in : "unknown");
            if (client->m_onError && in) {
                client->m_onError(std::string("Connection error: ") + (char*)in);
            }
            client->m_connected = false;
            
            {
                std::lock_guard<std::mutex> lock(s_instancesMutex);
                s_instances.erase(wsi);
            }
            
            if (client->m_running && client->m_shouldReconnect && !client->m_reconnecting) {
                client->StartReconnectTimer();
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            blog(LOG_INFO, "WebSocket connection closed");
            client->m_connected = false;
            
            if (client->m_onDisconnected) {
                client->m_onDisconnected();
            }
            
            {
                std::lock_guard<std::mutex> lock(s_instancesMutex);
                s_instances.erase(wsi);
            }
            
            if (client->m_running && client->m_shouldReconnect && !client->m_reconnecting) {
                client->StartReconnectTimer();
            }
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (client->m_onMessage && in && len > 0) {
                std::string message(static_cast<char*>(in), len);
                client->m_onMessage(message);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            {
                std::lock_guard<std::mutex> lock(client->m_writeMutex);
                
                if (client->m_currentSendBuffer) {
                    size_t msgLen = client->m_currentSendBuffer->data.size() - LWS_PRE;
                    int written = lws_write(wsi, 
                                          client->m_currentSendBuffer->data.data() + LWS_PRE,
                                          msgLen,
                                          LWS_WRITE_BINARY); // Using binary for audio data
                    
                    if (written < 0) {
                        blog(LOG_ERROR, "Failed to write to WebSocket");
                        return -1;
                    }
                    
                    client->m_currentSendBuffer.reset();
                }
                
                // Check if there are more messages to send
                {
                    std::lock_guard<std::mutex> sendLock(client->m_sendMutex);
                    if (!client->m_sendQueue.empty()) {
                        lws_callback_on_writable(wsi);
                    }
                }
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

void WebSocketClient::StartReconnectTimer() {
    if (m_reconnecting.exchange(true)) {
        return; // Already reconnecting
    }
    
    m_reconnectThread = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_reconnectDelay));
        
        if (!m_running || m_connected || !m_shouldReconnect) {
            m_reconnecting = false;
            return;
        }
        
        // Exponential backoff
        m_reconnectDelay = (std::min)(m_reconnectDelay * 2, m_maxReconnectDelay);
        
        // Clean up old context
        if (m_context) {
            lws_context_destroy(m_context);
            m_context = nullptr;
        }
        
        m_connected = false;
        m_running = false;
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
        
        // Clear send queue
        {
            std::lock_guard<std::mutex> lock(m_sendMutex);
            std::queue<std::string> empty;
            std::swap(m_sendQueue, empty);
        }
        
        if (m_shouldReconnect) {
            blog(LOG_INFO, "Attempting to reconnect to WebSocket server...");
            if (m_onError) {
                m_onError("Reconnecting...");
            }
            
            Connect(m_uri);
        }
        
        m_reconnecting = false;
    });
    
    m_reconnectThread.detach();
}

void WebSocketClient::StopReconnectTimer() {
    m_reconnecting = false;
    m_shouldReconnect = false;
}

} // namespace obs_audio_to_websocket
