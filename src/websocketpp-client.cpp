// CRITICAL: Ensure ASIO_STANDALONE is defined (should come from header)
#include "obs-audio-to-websocket/websocketpp-client.hpp"
#include <obs-module.h>
#include <nlohmann/json.hpp>
#include <asio/error_code.hpp>
#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/close.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/error.hpp>
#include <chrono>
#include <cstring>
#include <thread>
#include <websocketpp/common/functional.hpp>

namespace obs_audio_to_websocket {

using json = nlohmann::json;

WebSocketPPClient::WebSocketPPClient()
{
	// Clear all logs to avoid spam
	m_client.clear_access_channels(websocketpp::log::alevel::all);
	m_client.clear_error_channels(websocketpp::log::elevel::all);

	// Initialize ASIO
	m_client.init_asio();

	// Set up handlers - MUST use websocketpp::lib::bind!
	namespace lib = websocketpp::lib;
	m_client.set_open_handler(lib::bind(&WebSocketPPClient::OnOpen, this, lib::placeholders::_1));
	m_client.set_close_handler(lib::bind(&WebSocketPPClient::OnClose, this, lib::placeholders::_1));
	m_client.set_message_handler(
		lib::bind(&WebSocketPPClient::OnMessage, this, lib::placeholders::_1, lib::placeholders::_2));
	m_client.set_fail_handler(lib::bind(&WebSocketPPClient::OnFail, this, lib::placeholders::_1));
	// No TLS handler needed for ws_client
}

WebSocketPPClient::~WebSocketPPClient()
{
	Disconnect();
}

bool WebSocketPPClient::Connect(const std::string &uri)
{
	if (m_connected) {
		blog(LOG_WARNING, "[Audio to WebSocket] Already connected");
		return false;
	}

	m_uri = uri;

	// Start the event loop thread first
	if (!m_threadRunning) {
		m_threadRunning = true;
		m_running = true;
		m_thread = std::thread(&WebSocketPPClient::Run, this);
		// Give the thread time to start
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} else if (!m_running) {
		// Thread exists but event loop stopped, need to restart
		m_running = true;
		m_client.reset();
		// The thread should pick up that m_running is true again
	}

	try {
		websocketpp::lib::error_code ec;
		client::connection_ptr con = m_client.get_connection(uri, ec);

		if (ec) {
			blog(LOG_ERROR, "[Audio to WebSocket] Could not create connection: %s", ec.message().c_str());
			if (m_onError) {
				m_onError(ec.message());
			}
			return false;
		}

		{
			std::lock_guard<std::mutex> lock(m_hdlMutex);
			m_hdl = con->get_handle();
		}
		m_client.connect(con);

		return true;
	} catch (const websocketpp::exception &e) {
		blog(LOG_ERROR, "[Audio to WebSocket] Connection failed: %s", e.what());
		if (m_onError) {
			m_onError(e.what());
		}
		return false;
	}
}

void WebSocketPPClient::Disconnect()
{
	m_shouldReconnect = false;

	if (m_connected) {
		websocketpp::lib::error_code ec;
		websocketpp::connection_hdl hdl;
		{
			std::lock_guard<std::mutex> lock(m_hdlMutex);
			hdl = m_hdl;
		}
		m_client.close(hdl, websocketpp::close::status::normal, "Closing connection", ec);
		if (ec) {
			blog(LOG_ERROR, "[Audio to WebSocket] Error closing connection: %s", ec.message().c_str());
		}
	}

	// Stop the event loop
	m_running = false;
	m_client.stop();

	// Wait for thread to finish
	if (m_thread.joinable()) {
		m_thread.join();
		m_threadRunning = false;
	}

	m_connected = false;
}

void WebSocketPPClient::Run()
{

	int reset_count = 0;
	while (m_running) {
		try {

			m_client.run();

			// If we're still supposed to be running, the io_service ran out of work
			// Reset it so it can be run again
			if (m_running) {
				reset_count++;
				m_client.reset();
				// Small delay to prevent busy loop
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		} catch (const websocketpp::exception &e) {
			blog(LOG_ERROR, "[Audio to WebSocket] WebSocket++ exception: %s", e.what());
			// Continue loop unless we're shutting down
			if (m_running) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		} catch (const std::exception &e) {
			blog(LOG_ERROR, "[Audio to WebSocket] Exception: %s", e.what());
			if (m_running) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		} catch (...) {
			blog(LOG_ERROR, "[Audio to WebSocket] Unknown exception in WebSocket event loop");
			if (m_running) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}
	}

	m_threadRunning = false;
}

// ProcessSendQueue removed - we send messages directly now

void WebSocketPPClient::SendAudioData(const AudioChunk &chunk)
{
	if (!m_connected)
		return;

	// Convert to binary format (same as libwebsockets version)
	std::vector<uint8_t> binaryData;

	// Header: timestamp(8) + sampleRate(4) + channels(4) + bitDepth(4) + sourceIdLen(4) + sourceNameLen(4)
	size_t headerSize = 8 + 4 + 4 + 4 + 4 + 4;
	size_t sourceIdLen = chunk.sourceId.length();
	size_t sourceNameLen = chunk.sourceName.length();
	size_t totalSize = headerSize + sourceIdLen + sourceNameLen + chunk.data.size();

	binaryData.reserve(totalSize);

	// Write header (all multi-byte values in little-endian format)
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

	// Write audio data (16-bit signed PCM samples in little-endian format)
	binaryData.insert(binaryData.end(), chunk.data.begin(), chunk.data.end());

	// Send as binary message directly
	try {
		websocketpp::lib::error_code ec;
		websocketpp::connection_hdl hdl;
		{
			std::lock_guard<std::mutex> lock(m_hdlMutex);
			hdl = m_hdl;
		}
		m_client.send(hdl, binaryData.data(), binaryData.size(), websocketpp::frame::opcode::binary, ec);

		if (ec) {
			blog(LOG_ERROR, "[Audio to WebSocket] Failed to send audio data: %s", ec.message().c_str());
			// Mark as disconnected on send failure
			m_connected = false;
			if (m_onError) {
				m_onError("Failed to send audio data: " + ec.message());
			}
		}
	} catch (const websocketpp::exception &e) {
		blog(LOG_ERROR, "[Audio to WebSocket] Exception sending audio data: %s", e.what());
	}
}

void WebSocketPPClient::SendControlMessage(const std::string &type)
{
	if (!m_connected)
		return;

	json msg;
	msg["type"] = type;
	msg["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
				   std::chrono::system_clock::now().time_since_epoch())
				   .count();

	std::string payload = msg.dump();

	// Send control message directly
	try {
		websocketpp::lib::error_code ec;
		websocketpp::connection_hdl hdl;
		{
			std::lock_guard<std::mutex> lock(m_hdlMutex);
			hdl = m_hdl;
		}
		m_client.send(hdl, payload, websocketpp::frame::opcode::text, ec);

		if (ec) {
			blog(LOG_ERROR, "[Audio to WebSocket] Failed to send control message: %s",
			     ec.message().c_str());
		}
	} catch (const websocketpp::exception &e) {
		blog(LOG_ERROR, "[Audio to WebSocket] Exception sending control message: %s", e.what());
	}
}

void WebSocketPPClient::OnOpen(websocketpp::connection_hdl hdl)
{
	blog(LOG_INFO, "[Audio to WebSocket] Connected");
	{
		std::lock_guard<std::mutex> lock(m_hdlMutex);
		m_hdl = hdl; // Update the handle with the connected one
	}
	m_connected = true;

	if (m_onConnected) {
		m_onConnected();
	}

	// Send initial control message
	SendControlMessage("start");
}

void WebSocketPPClient::OnClose(websocketpp::connection_hdl hdl)
{
	(void)hdl; // Suppress unused parameter warning

	if (m_connected) {
		blog(LOG_INFO, "[Audio to WebSocket] Disconnected");
	}

	m_connected = false;

	if (m_onDisconnected) {
		m_onDisconnected();
	}

	// TODO: Implement reconnection logic if needed
}

void WebSocketPPClient::OnMessage(websocketpp::connection_hdl hdl, message_ptr msg)
{
	(void)hdl; // Suppress unused parameter warning
	if (m_onMessage) {
		m_onMessage(msg->get_payload());
	}
}

void WebSocketPPClient::OnFail(websocketpp::connection_hdl hdl)
{
	(void)hdl; // Suppress unused parameter warning

	// Get detailed error information
	client::connection_ptr con = m_client.get_con_from_hdl(hdl);
	websocketpp::lib::error_code ec = con->get_ec();

	blog(LOG_ERROR, "[Audio to WebSocket] Connection failed: %s (code: %d)", ec.message().c_str(), ec.value());
	m_connected = false;

	if (m_onError) {
		m_onError("Connection failed: " + ec.message());
	}
}

// TLS handler removed - using non-TLS client for now

} // namespace obs_audio_to_websocket
