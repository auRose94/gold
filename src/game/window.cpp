#include "window.hpp"

#include <iostream>
#include <map>
#include <memory>

#include "game/windowSystem.hpp"

namespace gold {
	using namespace std;

	obj& window::getPrototype() {
		static auto proto = obj{
			{"x", WindowCentered},
			{"y", WindowCentered},
			{"width", 1360},
			{"height", 800},
			{"maximize", false},
			{"fullscreen", false},
			{"borderless", false},
			{"matchDesktop", false},
			{"title", (char*)"RED2D"},
			{"backend", "sdl"},
			{"setSize", method(&window::setSize)},
			{"setPos", method(&window::setPos)},
			{"setTitle", method(&window::setTitle)},
			{"setFullscreen", method(&window::setFullscreen)},
			{"setBorderless", method(&window::setBorderless)},
			{"create", method(&window::create)},
			{"destroy", method(&window::destroy)},
			{"handleEvent", method(&window::handleEvent)},
			{"getConfig", method(&window::getConfig)},
			// Event handler slots (defaults update window state).
			{"onQuit", method(&window::onQuit)},
			{"onResized", method(&window::onResized)},
			{"onMoved", method(&window::onMoved)},
			{"onShown", method(&window::onShown)},
			{"onHidden", method(&window::onHidden)},
			{"onMinimized", method(&window::onMinimized)},
			{"onMaximized", method(&window::onMaximized)},
			{"onRestored", method(&window::onRestored)},
			{"onFocusGained", method(&window::onFocusGained)},
			{"onFocusLost", method(&window::onFocusLost)},
			{"onKeyDown", method(&window::onKeyDown)},
			{"onKeyUp", method(&window::onKeyUp)},
			{"onTextInput", method(&window::onTextInput)},
			{"onMouseDown", method(&window::onMouseDown)},
			{"onMouseUp", method(&window::onMouseUp)},
			{"onMouseMove", method(&window::onMouseMove)},
			{"onMouseWheel", method(&window::onMouseWheel)},
		};
		return proto;
	}

	auto windowConfigDefault = obj({
		{"x", WindowCentered},
		{"y", WindowCentered},
		{"width", 1360},
		{"height", 800},
		{"fullscreen", false},
		{"borderless", false},
		{"matchDesktop", false},
	});

	// gold's engine is single-window today, so one backend is shared across
	// copies of the window object (object copies share objData, not members).
	shared_ptr<windowSystem>& window::backend() {
		static shared_ptr<windowSystem> sys;
		return sys;
	}

	windowSystem* window::getBackend() {
		return backend().get();
	}

	// ---- setters ------------------------------------------------------

	var window::setSize(list args) {
		auto sys = backend();
		int32_t width = WindowCentered;
		int32_t height = WindowCentered;

		if (args.size() > 0 && args[0].getType() == typeList) {
			auto arr = args[0].getList();
			if (arr.getType(0) == typeInt32) width = arr.getInt32(0);
			if (arr.getType(1) == typeInt32) height = arr.getInt32(1);
		} else if (args.size() > 0 && args[0].isVec2()) {
			width = args[0].getInt32(0);
			height = args[0].getInt32(1);
		} else if (args.size() > 1 && args[0].isNumber() && args[1].isNumber()) {
			width = args[0].getInt32();
			height = args[1].getInt32();
		}
		setInt32("width", width);
		setInt32("height", height);
		if (sys) sys->setSize(width, height);
		return var();
	}

	var window::setPos(list args) {
		auto sys = backend();
		int32_t x = WindowCentered;
		int32_t y = WindowCentered;

		if (args.size() > 0 && args[0].getType() == typeList) {
			auto arr = args[0].getList();
			if (arr.getType(0) == typeInt32) x = arr.getInt32(0);
			if (arr.getType(1) == typeInt32) y = arr.getInt32(1);
		} else if (args.size() > 0 && args[0].isVec2()) {
			x = args[0].getInt32(0);
			y = args[0].getInt32(1);
		} else if (args.size() > 1 && args[0].isNumber() && args[1].isNumber()) {
			x = args[0].getInt32();
			y = args[1].getInt32();
		}
		setInt32("x", x);
		setInt32("y", y);
		if (sys) sys->setPos(x, y);
		return var();
	}

	var window::setTitle(list args) {
		auto sys = backend();
		string title;
		if (args.size() > 0 && args[0].getType() == typeString)
			title = args[0].getString();
		if (title.size() > 0) {
			setString("title", title);
			if (sys) sys->setTitle(title);
		} else
			setNull("title");
		return var();
	}

	var window::setFullscreen(list args) {
		auto sys = backend();
		bool fullscreen = false;
		bool desktop = false;
		if (args.size() > 0 && args[0].getType() == typeBool)
			fullscreen = (bool)args[0];
		else if (args.size() > 0 && args[0].getType() == typeList) {
			auto arr = args[0].getList();
			if (arr.getType(0) == typeBool)
				fullscreen = arr.getBool(0);
			if (arr.getType(1) == typeBool) desktop = arr.getBool(1);
		}
		setBool("fullscreen", fullscreen);
		setBool("desktop", desktop);
		if (sys) sys->setFullscreen(fullscreen, desktop);
		return var();
	}

	var window::setBorderless(list args) {
		auto sys = backend();
		bool borderless = false;
		if (args.size() > 0 && args[0].getType() == typeBool)
			borderless = args[0].getBool();
		setBool("borderless", borderless);
		if (sys) sys->setBorderless(borderless);
		return var();
	}

	var window::create(list) {
		destroy();

		// Config-driven backend selection with fallback. "backend" may be a
		// string name or a list of names tried in order; "headless" is always
		// appended as the last resort so apps never hard-fail on a missing
		// compositor.
		auto backendVar = getVar("backend");
		auto names = list();
		if (backendVar.isList()) {
			names = backendVar.getList();
		} else if (backendVar.isString()) {
			names.pushString(backendVar.getString());
		} else {
			names.pushString("sdl");
		}
		names.pushString("headless");

		auto sys = createWindowSystem(names);
		if (!sys) return genericError("No window system backend available");

		auto config = obj({
			{"x", getInt32("x", WindowCentered)},
			{"y", getInt32("y", WindowCentered)},
			{"width", getInt32("width", 1360)},
			{"height", getInt32("height", 800)},
			{"fullscreen", getBool("fullscreen", false)},
			{"borderless", getBool("borderless", false)},
			{"maximize", getBool("maximize", false)},
			{"desktop", getBool("desktop", false)},
			{"title", getString("title", "gold")},
		});
		if (!sys->create(config)) {
			delete sys;
			return genericError("Failed to create window");
		}
		setString("backend", sys->name());
		backend().reset(sys);
		return var();
	}

	var window::destroy(list) {
		auto sys = backend();
		if (sys) {
			sys->destroy();
			backend().reset();
		}
		return var();
	}

	// ---- event dispatch ------------------------------------------------

	var window::handleEvent(list args) {
		if (args.size() == 0 || args[0].getType() != typeObject) return var();
		auto ev = args[0].getObject();
		auto type = ev.getString("type");

		// Map event type to a handler slot name in camel case:
		// "key_down" -> "onKeyDown".
		string handlerName = "on";
		if (!type.empty()) {
			bool cap = true;
			for (char c : type) {
				if (c == '_') {
					cap = true;
				} else {
					handlerName += cap ? char(std::toupper((unsigned char)c))
													 : c;
					cap = false;
				}
			}
		}

		// Dispatch to the registered handler (method or func). Defaults live
		// on the prototype; apps override with setFunc/setMethod.
		auto handler = getVar(handlerName);
		if (handler.getType() == typeMethod || handler.getType() == typeFunction)
			return callMethod(handlerName, {ev});
		return var();
	}

	// ---- default handlers (update window state) ------------------------

	var window::onQuit(list) {
		setBool("quit", true);
		return var();
	}

	var window::onResized(list args) {
		auto ev = args[0].getObject();
		setInt32("width", ev.getInt32("width"));
		setInt32("height", ev.getInt32("height"));
		return var();
	}

	var window::onMoved(list args) {
		auto ev = args[0].getObject();
		setInt32("x", ev.getInt32("x"));
		setInt32("y", ev.getInt32("y"));
		return var();
	}

	var window::onShown(list) {
		setBool("hidden", false);
		return var();
	}

	var window::onHidden(list) {
		setBool("hidden", true);
		return var();
	}

	var window::onMinimized(list) {
		setBool("hidden", true);
		return var();
	}

	var window::onMaximized(list) {
		setBool("hidden", false);
		setBool("maximize", true);
		return var();
	}

	var window::onRestored(list) {
		setBool("hidden", false);
		return var();
	}

	var window::onFocusGained(list) {
		setBool("active", true);
		return var();
	}

	var window::onFocusLost(list) {
		setBool("active", false);
		return var();
	}

	var window::onKeyDown(list args) {
		auto ev = args[0].getObject();
		setInt32("keyCode", ev.getInt32("keyCode"));
		return var();
	}

	var window::onKeyUp(list args) {
		auto ev = args[0].getObject();
		setInt32("keyCode", ev.getInt32("keyCode"));
		return var();
	}

	var window::onTextInput(list args) {
		auto ev = args[0].getObject();
		setString("text", ev.getString("text"));
		return var();
	}

	var window::onMouseDown(list args) {
		auto ev = args[0].getObject();
		setInt32("x", ev.getInt32("x"));
		setInt32("y", ev.getInt32("y"));
		setInt32("button", ev.getInt32("button"));
		return var();
	}

	var window::onMouseUp(list args) {
		auto ev = args[0].getObject();
		setInt32("x", ev.getInt32("x"));
		setInt32("y", ev.getInt32("y"));
		setInt32("button", ev.getInt32("button"));
		return var();
	}

	var window::onMouseMove(list args) {
		auto ev = args[0].getObject();
		setInt32("x", ev.getInt32("x"));
		setInt32("y", ev.getInt32("y"));
		return var();
	}

	var window::onMouseWheel(list args) {
		auto ev = args[0].getObject();
		setInt32("scrollX", ev.getInt32("scrollX"));
		setInt32("scrollY", ev.getInt32("scrollY"));
		return var();
	}

	var window::getConfig(list) {
		auto allowed = windowConfigDefault;
		auto config = obj(windowConfigDefault);
		for (auto it = begin(); it != end(); ++it) {
			auto def = allowed[it->first];
			if (def.getType() != typeNull && it->second != def)
				config.setVar(it->first, it->second);
		}
		return config;
	}

	window::window() : obj() {}

	window::window(obj config) : obj() {
		copy(config);
		setParent(getPrototype());
	}

}  // namespace gold
