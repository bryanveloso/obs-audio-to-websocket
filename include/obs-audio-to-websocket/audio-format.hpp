#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obs_audio_to_websocket {

struct AudioFormat {
	uint32_t sampleRate;
	uint32_t channels;
	uint32_t bitDepth;

	AudioFormat(uint32_t sr = 48000, uint32_t ch = 2, uint32_t bd = 16) : sampleRate(sr), channels(ch), bitDepth(bd)
	{
	}
};

struct AudioChunk {
	std::vector<uint8_t> data;
	uint64_t timestamp;
	AudioFormat format;
	std::string sourceId;
	std::string sourceName;
};

inline std::string base64_encode(const uint8_t *data, size_t len)
{
	static const char *b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string result;
	result.reserve((len + 2) / 3 * 4);

	for (size_t i = 0; i < len; i += 3) {
		uint32_t n = (data[i] << 16) | (i + 1 < len ? data[i + 1] << 8 : 0) | (i + 2 < len ? data[i + 2] : 0);

		result += b64chars[(n >> 18) & 63];
		result += b64chars[(n >> 12) & 63];
		result += (i + 1 < len) ? b64chars[(n >> 6) & 63] : '=';
		result += (i + 2 < len) ? b64chars[n & 63] : '=';
	}

	return result;
}

} // namespace obs_audio_to_websocket
