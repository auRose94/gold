#pragma once

#include <cstdint>
#include <string>

#include "types.hpp"

namespace gold {

	/**
	 * Abstract audio backend. Plays sound effects/music loaded from files
	 * or raw buffers. Backends are registered by name (e.g. "sdl") and
	 * created via createAudioSystem().
	 */
	class audioSystem {
	 public:
		virtual ~audioSystem() = default;

		/** Initialize the audio device. Returns false on failure. */
		virtual bool open() = 0;
		virtual void close() = 0;

		/** Load a sound file (WAV). Returns a nonzero handle, or 0 on failure. */
		virtual uint64_t loadSound(const std::string& path) = 0;
		virtual void unloadSound(uint64_t handle) = 0;

		/** Play a loaded sound. Returns false on failure. */
		virtual bool play(uint64_t handle, bool loop = false) = 0;

		/** Stop all currently playing sounds. */
		virtual void stop() = 0;

		/** Master volume in [0, 1]. */
		virtual void setVolume(float volume) = 0;

		/** Backend name, e.g. "sdl". */
		virtual const char* name() const = 0;
	};

	/** Create an audio backend by name. */
	audioSystem* createAudioSystem(const std::string& name);
	void registerAudioSystem(const std::string& name,
		audioSystem* (*factory)());

}  // namespace gold