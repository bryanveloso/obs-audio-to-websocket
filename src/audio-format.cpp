#include "obs-audio-to-websocket/audio-format.hpp"

namespace obs_audio_to_websocket {

AudioFormat::AudioFormat(uint32_t sr, uint32_t ch, uint32_t bd) : sampleRate(sr), channels(ch), bitDepth(bd)
{
	// Trust OBS to provide valid formats
}

bool AudioFormat::isValid() const
{
	// Simple check - trust OBS
	return channels > 0 && sampleRate > 0 && bitDepth > 0;
}

} // namespace obs_audio_to_websocket
