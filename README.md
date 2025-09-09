# OBS Audio to WebSocket Plugin

[![Build Status](https://github.com/bryanveloso/obs-audio-to-websocket/actions/workflows/push.yaml/badge.svg)](https://github.com/bryanveloso/obs-audio-to-websocket/actions/workflows/push.yaml)

A lightweight OBS Studio plugin that streams real-time audio data from OBS sources to WebSocket endpoints for remote processing and analysis.

## Features

- Stream audio from any OBS audio source to WebSocket endpoints
- Automatic reconnection with exponential backoff
- Auto-connect on OBS startup (optional setting)
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
5. (Optional) Enable "Auto-Connect on Startup" to automatically start streaming when OBS launches
6. Click "Connect" to establish WebSocket connection
7. Click "Start Streaming" to begin audio streaming

### Auto-Connect Feature

The auto-connect feature allows the plugin to automatically establish a WebSocket connection and begin streaming whenever OBS starts up. This is useful for automated setups where you want audio streaming to begin without manual intervention.

To enable auto-connect:
1. Open Tools → Audio to WebSocket Settings
2. Check the "Auto-Connect on Startup" checkbox
3. Click "Save" or close the dialog

When enabled, the plugin will:
- Automatically connect to the configured WebSocket URL on OBS startup
- Begin streaming audio from the selected source immediately
- Show connection status in the OBS log

**Note**: Auto-connect only works if you have previously configured a valid WebSocket URL and audio source.

## WebSocket Protocol

The plugin sends audio data as **binary WebSocket messages** with the following format:

### Binary Message Structure

All multi-byte values are in **little-endian** format.

#### Header (28 bytes)
| Offset | Size | Type   | Description |
|--------|------|--------|-------------|
| 0      | 8    | uint64 | Timestamp (nanoseconds since epoch) |
| 8      | 4    | uint32 | Sample rate (Hz, e.g., 48000) |
| 12     | 4    | uint32 | Channel count (e.g., 2 for stereo) |
| 16     | 4    | uint32 | Bit depth (always 16) |
| 20     | 4    | uint32 | Source ID string length |
| 24     | 4    | uint32 | Source name string length |

#### Variable Length Data
| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 28     | Variable | UTF-8 | Source ID (no null terminator) |
| 28 + sourceIdLen | Variable | UTF-8 | Source name (no null terminator) |
| 28 + sourceIdLen + sourceNameLen | Remaining | Binary | 16-bit signed PCM audio data (little-endian, interleaved) |

### Audio Data Format
- **Format**: 16-bit signed PCM (NOT float32 or unsigned)
- **Byte Order**: Little-endian
- **Channel Layout**: Interleaved (L,R,L,R,... for stereo)
- **Sample Range**: -32768 to 32767

### Example Client Implementation

#### JavaScript/Node.js
```javascript
ws.on('message', (data) => {
  if (typeof data === 'string') {
    // Control messages (JSON)
    const msg = JSON.parse(data);
    console.log('Control message:', msg.type);
    return;
  }
  
  // Binary audio data
  const buffer = Buffer.from(data);
  let offset = 0;
  
  // Read header
  const timestamp = buffer.readBigUInt64LE(offset); offset += 8;
  const sampleRate = buffer.readUInt32LE(offset); offset += 4;
  const channels = buffer.readUInt32LE(offset); offset += 4;
  const bitDepth = buffer.readUInt32LE(offset); offset += 4;
  const sourceIdLen = buffer.readUInt32LE(offset); offset += 4;
  const sourceNameLen = buffer.readUInt32LE(offset); offset += 4;
  
  // Read strings
  const sourceId = buffer.toString('utf8', offset, offset + sourceIdLen);
  offset += sourceIdLen;
  const sourceName = buffer.toString('utf8', offset, offset + sourceNameLen);
  offset += sourceNameLen;
  
  // Audio data is the rest
  const audioData = buffer.slice(offset);
  
  // Process 16-bit signed PCM samples
  const samples = new Int16Array(audioData.buffer, audioData.byteOffset, audioData.length / 2);
  
  console.log(`Audio: ${sampleRate}Hz, ${channels}ch, ${samples.length} samples`);
});
```

#### Python
```python
import struct
import numpy as np

def parse_audio_message(data):
    offset = 0
    
    # Parse header (little-endian)
    timestamp = struct.unpack('<Q', data[offset:offset+8])[0]; offset += 8
    sample_rate = struct.unpack('<I', data[offset:offset+4])[0]; offset += 4
    channels = struct.unpack('<I', data[offset:offset+4])[0]; offset += 4
    bit_depth = struct.unpack('<I', data[offset:offset+4])[0]; offset += 4
    source_id_len = struct.unpack('<I', data[offset:offset+4])[0]; offset += 4
    source_name_len = struct.unpack('<I', data[offset:offset+4])[0]; offset += 4
    
    # Parse strings
    source_id = data[offset:offset+source_id_len].decode('utf-8')
    offset += source_id_len
    source_name = data[offset:offset+source_name_len].decode('utf-8')
    offset += source_name_len
    
    # Parse audio data as 16-bit signed integers
    audio_bytes = data[offset:]
    audio_samples = np.frombuffer(audio_bytes, dtype='<i2')  # little-endian int16
    
    # Reshape if stereo
    if channels == 2:
        audio_samples = audio_samples.reshape(-1, 2)
    
    return {
        'timestamp': timestamp,
        'sample_rate': sample_rate,
        'channels': channels,
        'samples': audio_samples
    }
```

### Control Messages (JSON)
The plugin also sends JSON control messages:
```json
{
  "type": "start",
  "timestamp": 1234567890123456
}
```

```json
{
  "type": "stop",
  "timestamp": 1234567890123456
}
```

## Configuration

Settings are automatically saved in OBS configuration:
- WebSocket URL (default: `ws://localhost:8889/audio`)
- Selected audio source
- Auto-connect on startup setting
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