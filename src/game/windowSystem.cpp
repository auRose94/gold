#include "game/windowSystem.hpp"

#include <map>
#include <mutex>

#include "module.hpp"
#include "plugin.hpp"

namespace gold {

	namespace {
		std::map<std::string, createWindowSystemFn>& factories() {
			static std::map<std::string, createWindowSystemFn> f;
			return f;
		}
		std::mutex& factoryMutex() {
			static std::mutex m;
			return m;
		}
		applicationDataDirFn& dataDirFn() {
			static applicationDataDirFn fn = nullptr;
			return fn;
		}
	}  // namespace

	object eventToObject(const windowEvent& ev) {
		object out;
		const char* type = "none";
		switch (ev.type) {
			case windowEvent::eventType::Quit:
				type = "quit";
				break;
			case windowEvent::eventType::Resized:
				type = "resized";
				out.setInt32("width", ev.width);
				out.setInt32("height", ev.height);
				break;
			case windowEvent::eventType::Moved:
				type = "moved";
				out.setInt32("x", ev.x);
				out.setInt32("y", ev.y);
				break;
			case windowEvent::eventType::Shown:
				type = "shown";
				break;
			case windowEvent::eventType::Hidden:
				type = "hidden";
				break;
			case windowEvent::eventType::Minimized:
				type = "minimized";
				break;
			case windowEvent::eventType::Maximized:
				type = "maximized";
				break;
			case windowEvent::eventType::Restored:
				type = "restored";
				break;
			case windowEvent::eventType::FocusGained:
				type = "focus_gained";
				break;
			case windowEvent::eventType::FocusLost:
				type = "focus_lost";
				break;
			case windowEvent::eventType::KeyDown:
				type = "key_down";
				out.setInt32("keyCode", ev.keyCode);
				break;
			case windowEvent::eventType::KeyUp:
				type = "key_up";
				out.setInt32("keyCode", ev.keyCode);
				break;
			case windowEvent::eventType::TextInput:
				type = "text_input";
				out.setString("text", ev.text);
				break;
			case windowEvent::eventType::MouseDown:
				type = "mouse_down";
				out.setInt32("x", ev.x);
				out.setInt32("y", ev.y);
				out.setInt32("button", ev.button);
				break;
			case windowEvent::eventType::MouseUp:
				type = "mouse_up";
				out.setInt32("x", ev.x);
				out.setInt32("y", ev.y);
				out.setInt32("button", ev.button);
				break;
			case windowEvent::eventType::MouseMove:
				type = "mouse_move";
				out.setInt32("x", ev.x);
				out.setInt32("y", ev.y);
				break;
			case windowEvent::eventType::MouseWheel:
				type = "mouse_wheel";
				out.setInt32("scrollX", ev.scrollX);
				out.setInt32("scrollY", ev.scrollY);
				break;
			default:
				break;
		}
		out.setString("type", type);
		return out;
	}

	void registerWindowSystem(const string& name, createWindowSystemFn fn) {
		std::lock_guard<std::mutex> guard(factoryMutex());
		factories()[name] = fn;
	}

	void registerApplicationDataDir(applicationDataDirFn fn) {
		dataDirFn() = fn;
	}

	windowSystem* createWindowSystem(const string& name) {
		{
			std::lock_guard<std::mutex> guard(factoryMutex());
			auto it = factories().find(name);
			if (it != factories().end()) return it->second();
		}
		// Not statically registered: try loading it as a shared module. A
		// backend plugin self-registers at load time via its static
		// initializers, so loading is all that is needed. Load outside the
		// registry mutex: the plugin's own registration takes it too.
		plugin::load(name);
		std::lock_guard<std::mutex> guard(factoryMutex());
		auto it = factories().find(name);
		if (it != factories().end()) return it->second();
		return nullptr;
	}

	windowSystem* createWindowSystem(const list& names) {
		auto copy = names;
		for (auto it = copy.begin(); it != copy.end(); ++it) {
			auto name = it->getString();
			if (name.empty()) continue;
			if (auto sys = createWindowSystem(name)) return sys;
		}
		return nullptr;
	}

	string applicationDataDir(const string& company, const string& gameName) {
		if (dataDirFn()) return dataDirFn()(company, gameName);
		// Portable default: current directory.
		return "./";
	}

}  // namespace gold