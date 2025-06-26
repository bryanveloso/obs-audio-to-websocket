# OBS Audio to WebSocket Plugin

A lightweight OBS Studio plugin that streams real-time audio data from OBS sources to WebSocket endpoints for remote processing and analysis.

## Features

- Stream audio from any OBS audio source to WebSocket endpoints
- Automatic reconnection with exponential backoff
- Binary protocol for efficient audio data transmission
- Real-time connection status and data rate monitoring
- Simple UI integrated into OBS Tools menu
- Support for multiple audio formats (48kHz, 44.1kHz, etc.)

## System Requirements

### For Users
- OBS Studio 31.0 or higher
- Windows 10/11, macOS 11+, or Linux

### For Building from Source
- CMake 3.28 or higher
- Qt6
- WebSocket++ 0.8.2
- Asio 1.12.1 (standalone)
- nlohmann/json
- C++17 compatible compiler

## Quick Start

1. Download the latest release for your platform from the [Releases page](https://github.com/bryanveloso/obs-audio-to-websocket/releases)
2. Install the plugin (see Installation section below)
3. Restart OBS Studio
4. Go to Tools → Audio to WebSocket Settings
5. Enter your WebSocket server URL and select an audio source
6. Click Connect and Start Streaming

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

#### Platform-Specific Setup

**Windows:**
- Dependencies are automatically downloaded and built by the build scripts
- No manual installation required

**macOS:**
```bash
brew install nlohmann-json
# WebSocket++ and Asio will be automatically downloaded if not found
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install nlohmann-json3-dev
# WebSocket++ and Asio will be automatically downloaded if not found
```

#### Build Commands

```bash
# Clone the repository
git clone https://github.com/bryanveloso/obs-audio-to-websocket.git
cd obs-audio-to-websocket

# Configure and build
cmake --preset windows-x64           # For Windows x64
cmake --preset macos-arm64          # For macOS (Apple Silicon)
cmake --preset ubuntu-x86_64        # For Linux x64

cmake --build --preset windows-x64   # Build for Windows
cmake --build --preset macos-arm64  # Build for macOS
cmake --build --preset ubuntu-x86_64 # Build for Linux
```

The built plugin will be in the `release` folder.

### GitHub Actions

The project includes GitHub Actions workflows for automated building:
- Push to `main` branch triggers builds for all platforms
- Pull requests automatically build and test changes
- Build artifacts are available for download from successful workflow runs

## Installation

1. Download the appropriate installer or archive from the [Releases page](https://github.com/bryanveloso/obs-audio-to-websocket/releases)
2. Install using the method for your platform:

   **Windows**: 
   - Run the installer (.exe) if available, or
   - Extract the .zip and copy:
     - `obs-audio-to-websocket.dll` to `C:\Program Files\obs-studio\obs-plugins\64bit\`
     - Data files to `C:\Program Files\obs-studio\data\obs-plugins\obs-audio-to-websocket\`
   
   **macOS**: 
   - Run the installer (.pkg) if available, or
   - Extract and copy the `.plugin` bundle to `~/Library/Application Support/obs-studio/plugins/`
   
   **Linux**: 
   - Install the .deb package (Ubuntu/Debian), or
   - Extract and copy to `/usr/share/obs/obs-plugins/` or `~/.config/obs-studio/plugins/`

## Usage

1. Launch OBS Studio
2. Go to Tools → Audio to WebSocket Settings
3. Configure the WebSocket endpoint (default: `ws://localhost:8889/audio`)
4. Select your audio source
5. Click "Connect" to establish WebSocket connection
6. Click "Start Streaming" to begin audio streaming

## WebSocket Protocol

The plugin uses a binary protocol for efficient audio transmission:

### Binary Audio Data Format
- Header (28 bytes):
  - `timestamp` (8 bytes): Microseconds since epoch
  - `sampleRate` (4 bytes): Sample rate in Hz
  - `channels` (4 bytes): Number of audio channels
  - `bitDepth` (4 bytes): Bits per sample
  - `sourceIdLen` (4 bytes): Length of source ID string
  - `sourceNameLen` (4 bytes): Length of source name string
- Variable data:
  - Source ID string
  - Source name string
  - Raw PCM audio data

### Control Messages (JSON)
```json
{
  "type": "start",
  "timestamp": 1234567890123456
}
```

## Configuration

Settings are automatically saved in OBS configuration:
- WebSocket URL (default: `ws://localhost:8889/audio`)
- Selected audio source
- Connection state is maintained across OBS restarts

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
- Ensure CMake 3.28+ is installed
- Verify your compiler supports C++17
- On Windows: Visual Studio 2022 with C++ workload required
- On macOS: Xcode 14+ with command line tools
- Dependencies are automatically downloaded during build

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

This project is licensed under the GPL-2.0 License - same as OBS Studio.