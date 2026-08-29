#pragma once

#include <memory>
#include <string>
#include <vector>

#include "types.hpp"
#include "game/windowSystem.hpp"

namespace gold {

	/**
	 * Abstract input device backend. Captures real input from platform
	 * devices (evdev on Linux, etc.) and turns them into the same
	 * backend-agnostic windowEvent stream the window system produces.
	 *
	 * A uinput device is write-only (it *injects* events into the kernel),
	 * so "real input capture" is implemented with evdev/libinput instead.
	 */
	class inputSystem {
	 public:
		virtual ~inputSystem() = default;

		/** Open a set of device paths (e.g. /dev/input/event*). */
		virtual bool open(std::vector<std::string> paths) = 0;
		virtual void close() = 0;

		/** Pump one input event. Returns false when none remain. */
		virtual bool poll(windowEvent& out) = 0;

		/** Backend name, e.g. "evdev". */
		virtual const char* name() const = 0;
	};

	/** Create an input backend by name. */
	inputSystem* createInputSystem(const std::string& name);
	void registerInputSystem(const std::string& name,
		inputSystem* (*factory)());

}  // namespace gold