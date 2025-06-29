#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obs_audio_to_websocket {

struct AudioFormat {
	uint32_t sampleRate;
	uint32_t channels;
	uint32_t bitDepth;

	AudioFormat(uint32_t sr = 48000, uint32_t ch = 2, uint32_t bd = 16);

	bool isValid() const;
};

struct AudioChunk {
	std::vector<uint8_t> data;
	uint64_t timestamp;
	AudioFormat format;
	std::string sourceId;
	std::string sourceName;
};

} // namespace obs_audio_to_websocket
