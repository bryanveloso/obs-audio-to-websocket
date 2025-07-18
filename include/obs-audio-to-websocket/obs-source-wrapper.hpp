#pragma once

#include <obs.h>
#include <string>
#include <utility>

namespace obs_audio_to_websocket {

// RAII wrapper for OBS sources
class OBSSourceWrapper {
public:
	OBSSourceWrapper() = default;

	explicit OBSSourceWrapper(const std::string &name) : m_source(obs_get_source_by_name(name.c_str())) {}

	explicit OBSSourceWrapper(obs_source_t *source, bool add_ref = true) : m_source(source)
	{
		if (m_source && add_ref) {
			m_source = obs_source_get_ref(m_source);
		}
	}

	~OBSSourceWrapper() { reset(); }

	// Delete copy constructor and assignment
	OBSSourceWrapper(const OBSSourceWrapper &) = delete;
	OBSSourceWrapper &operator=(const OBSSourceWrapper &) = delete;

	// Move constructor
	OBSSourceWrapper(OBSSourceWrapper &&other) noexcept : m_source(std::exchange(other.m_source, nullptr)) {}

	// Move assignment
	OBSSourceWrapper &operator=(OBSSourceWrapper &&other) noexcept
	{
		if (this != &other) {
			reset();
			m_source = std::exchange(other.m_source, nullptr);
		}
		return *this;
	}

	// Get raw pointer (for OBS API calls)
	obs_source_t *get() const { return m_source; }

	// Boolean conversion
	explicit operator bool() const { return m_source != nullptr; }

	// Arrow operator for convenience
	obs_source_t *operator->() const { return m_source; }

	// Release ownership without releasing the source
	obs_source_t *release() { return std::exchange(m_source, nullptr); }

	// Reset with optional new source
	void reset(obs_source_t *source = nullptr, bool add_ref = true)
	{
		if (m_source) {
			obs_source_release(m_source);
		}
		m_source = source;
		if (m_source && add_ref) {
			m_source = obs_source_get_ref(m_source);
		}
	}

	// Helper methods
	std::string get_name() const
	{
		if (!m_source)
			return "";
		const char *name = obs_source_get_name(m_source);
		return name ? name : "";
	}

	bool is_audio_source() const
	{
		if (!m_source)
			return false;
		uint32_t flags = obs_source_get_output_flags(m_source);
		return (flags & OBS_SOURCE_AUDIO) != 0;
	}

private:
	obs_source_t *m_source = nullptr;
};

} // namespace obs_audio_to_websocket
