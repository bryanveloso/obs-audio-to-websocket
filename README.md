# OBS Audio to WebSocket Plugin

A lightweight OBS Studio plugin that streams audio from specific sources to WebSocket endpoints for remote processing.

## Features

- Select and stream audio from any OBS audio source
- WebSocket streaming with automatic reconnection
- Low latency audio streaming (<100ms)
- Real-time data rate monitoring
- Simple UI integrated into OBS Tools menu

## Requirements

- OBS Studio 30.x
- CMake 3.16 or higher
- Qt6
- Boost (for WebSocket++)
- C++17 compatible compiler

## Building

### Windows

```bash
# Clone the repository
git clone https://github.com/yourusername/obs-audio-to-websocket.git
cd obs-audio-to-websocket

# Download dependencies
# WebSocket++ (header-only library)
git clone https://github.com/zaphoyd/websocketpp.git deps/websocketpp

# nlohmann/json (header-only library)
mkdir -p deps/json/include/nlohmann
curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o deps/json/include/nlohmann/json.hpp

# Configure with CMake
cmake -B build -S . -DCMAKE_PREFIX_PATH="path/to/obs-studio/build" -DQt6_DIR="path/to/Qt/6.x/msvc2019_64/lib/cmake/Qt6"

# Build
cmake --build build --config Release

# Install
cmake --install build --config Release
```

### macOS/Linux

```bash
# Install dependencies
# macOS: brew install boost qt@6
# Linux: sudo apt-get install libboost-all-dev qt6-base-dev

# Clone and build
git clone https://github.com/yourusername/obs-audio-to-websocket.git
cd obs-audio-to-websocket

# Get dependencies
git clone https://github.com/zaphoyd/websocketpp.git deps/websocketpp
mkdir -p deps/json/include/nlohmann
curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o deps/json/include/nlohmann/json.hpp

# Configure and build
cmake -B build -S . -DCMAKE_PREFIX_PATH="/path/to/obs-studio/build"
cmake --build build
sudo cmake --install build
```

## Usage

1. Launch OBS Studio
2. Go to Tools → Audio to WebSocket Settings
3. Configure the WebSocket endpoint (default: `ws://localhost:8889/audio`)
4. Select your audio source
5. Click "Connect" to establish WebSocket connection
6. Click "Start Streaming" to begin audio streaming

## WebSocket Protocol

The plugin sends JSON messages with the following format:

```json
{
  "type": "audio_data",
  "timestamp": 1234567890123456,
  "format": {
    "sampleRate": 48000,
    "channels": 2,
    "bitDepth": 16
  },
  "data": "base64_encoded_pcm_data",
  "sourceId": "mic_aux_1",
  "sourceName": "Microphone"
}
```

## Configuration

Settings are saved in OBS configuration:
- WebSocket URL
- Selected audio source

## Troubleshooting

### Connection Issues
- Ensure the WebSocket server is running and accessible
- Check firewall settings
- Verify the URL format (ws:// or wss://)

### Audio Issues
- Ensure the audio source is active in OBS
- Check that the source is not muted
- Verify audio levels in OBS mixer

### Build Issues
- Make sure all dependencies are installed
- Use the correct CMake prefix paths for OBS and Qt
- Check compiler C++17 support

## License

This project is licensed under the GPL-2.0 License - same as OBS Studio.