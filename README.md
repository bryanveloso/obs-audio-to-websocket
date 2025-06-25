# OBS Audio to WebSocket Plugin

A lightweight OBS Studio plugin that streams audio from specific sources to WebSocket endpoints for remote processing.

## Features

- Select and stream audio from any OBS audio source
- WebSocket streaming with automatic reconnection
- Low latency audio streaming (<100ms)
- Real-time data rate monitoring
- Simple UI integrated into OBS Tools menu

## Requirements

- OBS Studio 31.x
- CMake 3.28 or higher
- Qt6
- libwebsockets
- C++17 compatible compiler

## Building

This plugin uses the official OBS plugin template build system.

### Prerequisites

1. Install Git
2. Install CMake 3.28 or higher
3. Install compiler toolchain:
   - **Windows**: Visual Studio 2022
   - **macOS**: Xcode 14.0 or higher
   - **Linux**: GCC 11 or higher

### Building from Source

#### Additional Dependencies

This plugin requires some dependencies beyond what OBS provides:

**Windows:**
- Install libwebsockets and nlohmann/json (recommended: use vcpkg or build from source)
- Add the installation path to CMAKE_PREFIX_PATH when configuring

**macOS:**
```bash
brew install libwebsockets nlohmann-json
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libwebsockets-dev nlohmann-json3-dev
```

#### Build Commands

```bash
# Clone the repository
git clone https://github.com/bryanveloso/obs-audio-to-websocket.git
cd obs-audio-to-websocket

# Configure
cmake --preset windows-x64     # For Windows
cmake --preset macos           # For macOS
cmake --preset ubuntu-x86_64   # For Linux

# Build
cmake --build --preset windows-x64     # For Windows
cmake --build --preset macos           # For macOS
cmake --build --preset ubuntu-x86_64   # For Linux
```

The plugin will be built in the `release` folder.

### GitHub Actions

The project includes GitHub Actions workflows for automated building. Simply push to main or create a pull request to trigger builds for all platforms.

## Installation

1. Download the latest release for your platform
2. Extract the archive
3. Copy the plugin files to your OBS installation:
   - **Windows**: Copy to `C:\Program Files\obs-studio\obs-plugins\64bit`
   - **macOS**: Copy to `/Library/Application Support/obs-studio/plugins`
   - **Linux**: Copy to `/usr/share/obs/obs-plugins`

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
- Make sure you have the required CMake version (3.28+)
- Ensure your compiler supports C++17
- Dependencies are automatically managed by the build system

## License

This project is licensed under the GPL-2.0 License - same as OBS Studio.