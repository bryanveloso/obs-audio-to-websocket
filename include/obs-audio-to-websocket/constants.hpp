#pragma once

namespace obs_audio_to_websocket {
namespace constants {

// Reconnection settings - these are user-facing and worth keeping as constants
constexpr int MAX_RECONNECT_ATTEMPTS = 10;
constexpr int INITIAL_RECONNECT_DELAY_MS = 1000; // 1 second
constexpr int MAX_RECONNECT_DELAY_MS = 30000;    // 30 seconds

} // namespace constants
} // namespace obs_audio_to_websocket
