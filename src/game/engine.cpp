
#include "engine.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "camera.hpp"
#include "component.hpp"
#include "entity.hpp"
#include "envMap.hpp"
#include "game/windowSystem.hpp"
#include "goldjs.hpp"
#include "graphics.hpp"
#include "light.hpp"
#include "mcpServerSystem.hpp"
#include "meshRenderer.hpp"
#include "promise.hpp"
#include "window.hpp"
#include "world.hpp"

namespace gold {
	using namespace std;
	object& engine::getPrototype() {
		static auto proto = object({
			{"initialize", method(&engine::initialize)},
			{"start", method(&engine::start)},
			{"loadSettings", method(&engine::loadSettings)},
			{"saveSettings", method(&engine::saveSettings)},
		});
		return proto;
	}

	string engine::getSettingsDir() {
		auto gameName = getString("gameName");
		auto company = getString("company");
		auto dir = applicationDataDir(company, gameName);
		if (dir.size() == 0) dir = "./";
		return dir;
	}

	string engine::getSettingsPath() {
		return string(engine::getSettingsDir() + "config.json");
	}

	var engine::loadSettings(list) {
		auto configPath = getSettingsPath();
		auto configJSON = object::loadJSON(configPath);
		object config;
		if (configJSON.isObject()) {
			auto o = configJSON.getObject();
			config = o ? object(o) : config;
			cout << "Loading settings: " << configPath << endl;
		} else {
			config = obj({});
		}
		setObject("config", config);
		return config;
	}

	var engine::saveSettings(list) {
		auto config = getObject("config");
		auto win = getObject<window>("window");
		auto gfx = getObject<gfxBackend>("graphics");
		if (win && gfx) {
			auto windowConfigVar = win.getConfig({});
			auto windowConfig = windowConfigVar.getObject();
			if (windowConfig) {
				config.setObject("window", windowConfig);
			} else if (windowConfigVar.isError())
				return windowConfigVar;

			auto gfxConfigVar = gfx.getConfig();
			auto gfxConfig = gfxConfigVar.getObject();
			if (gfxConfig && gfxConfig.size() > 0) {
				config.setObject("graphics", gfxConfig);
			} else if (gfxConfigVar.isError())
				return gfxConfigVar;

			auto configPath = getSettingsPath();
			object::saveJSON(configPath, config);
		}
		return var();
	}

	var engine::getPrimaryCamera(list) {
		auto cameras = getList("cameras");
		for (auto it = cameras.begin(); it != cameras.end(); ++it) {
			auto cam = it->getObject<camera>();
			if (cam && cam.getUInt16("view") == 0) return *it;
		}
		return var();
	}

	namespace {
	/** The console-argument backend flags land after the settings file,
	 *  so the command line wins: defaults < config.json < argv. Each
	 *  override key overwrites the same key in the config's section. */
	void applyConsoleOverrides(object& config, object& overrides) {
		for (auto sectionName : engine::allowedConfigNames()) {
			auto sectionOverrides = overrides.getObject(sectionName);
			if (!sectionOverrides) continue;
			auto section = config.getObject(sectionName, obj({}));
			for (auto it = sectionOverrides.begin();
					 it != sectionOverrides.end(); ++it) {
				section[it->first] = it->second;
				cout << "Backend override (console): " << sectionName
					<< "." << it->first << " = "
					<< (it->second.isList() ? string("(fallback chain)")
											: it->second.getString())
					<< endl;
			}
			config.setObject(sectionName, section);
		}
	}
}  // namespace

var engine::initialize(list) {
		setList("entities", list({}));

		// Warm the backend registries before the settings load: the SDL
		// plugin's registrar provides the application data directory the
		// settings/saves live in, and it loads on first backend use —
		// without this warmup the settings file was silently unreachable.
		// (The instance is throwaway; nothing is initialized until
		// create().)
		if (auto* warmup = createWindowSystem(list({"sdl", "headless"})))
			delete warmup;

		auto configVar = loadSettings();
		auto config = configVar.getObject();
		// The console-argument flags apply last so they win over the
		// settings file.
		if (auto overrides = getObject("backendOverrides"))
			applyConsoleOverrides(config, overrides);
		auto gameName = getString("gameName");

		auto windowConfig = config.getObject("window", obj({}));
		auto backendConfig = config.getObject("graphics", obj({}));
		auto win = create<window>("window", windowConfig);
		win.setTitle({gameName});
		win.create();
		auto gfx = create<gfxBackend>("graphics", backendConfig);
		gfx.initialize({win});
		auto phys = world(obj{});
		phys.initialize({*this});
		setObject("world", phys);

		auto w = win.getFloat("width");
		auto h = win.getFloat("height");

		setList(
			"cameras",
			list({
				camera({
					{"size", vec2f(w, h)},
				}),
			}));

		// The MCP development server ("mcp" config section): a config-
		// activated debug surface over this running engine, implemented
		// by the loadable mcp module (HTTP via the web module's
		// transports). When config asked for it but no module is built,
		// say so and keep running.
		if (auto mcpConfig = config.getObject("mcp", obj({}));
			mcpConfig && mcpConfig.getBool("enabled", false)) {
			auto* mcp = createMcpServer(ja("mcp"));
			if (mcp) {
				mcp->initialize(mcpConfig);
				object roots;
				roots.setObject("engine", *this);
				mcp->setRoots(roots);
				setPtr("mcpSystem", (void*)mcp);
			} else {
				cout << "Config requested the MCP server, but no mcp "
						"module is available (build with the web module "
						"and the lws transport)."
					<< endl;
			}
		}

		return var();
	}

	void awaitList(list& promises) {
		for (auto it = promises.begin(); it != promises.end();
				 ++it) {
			auto job = it->getObject<promise>();
			job.await();
		}
		promises.resize(0);
	}

	void engine::sortComponents() {
		auto comps = getList("components");
		comps.sort([](var a, var b) {
			auto objA = a.getObject();
			auto objB = b.getObject();
			auto aP = objA.getUInt64("priority");
			auto bP = objB.getUInt64("priority");
			return aP < bP;
		});
	}

	void engine::initComps() {
		auto comps = getList("components");
		for (auto it = comps.begin(); it != comps.end(); ++it) {
			if (it != comps.end() && it->isObject()) {
				auto comp = it->getObject<component>();
				if (!comp.getBool("_inited")) {
					comp.callMethod("initialize");
					comp.setBool("_inited", true);
				}
			}
		}
	}

	namespace {
		/** A component whose owning entity chain ("object" ref and its
		 *  "parent" chain) has a disabled ancestor skips update/draw
		 *  dispatch. Loose components (no owning entity) always run. */
		bool gatedByDisabledEntity(object& comp) {
			auto owner = comp.getObject<entity>("object");
			if (!owner) return false;
			auto cur = owner;
			for (;;) {
				if (!cur.getBool("enabled", true)) return true;
				auto parent = cur.getObject<entity>("parent");
				if (!parent) return false;
				cur = parent;
			}
		}
	}  // namespace

	void engine::callMethod(string m, list args) {
		auto comps = getList("components");
		// Parallel dispatch of component updates across the promise worker
		// pool (on by default; disable with config "parallelUpdate": false).
		// Requires promise::useAllCores() to have been called; otherwise
		// awaitList() runs each update synchronously (same as the default
		// path). Draw and other methods stay on the main thread.
		auto config = getObject("config");
		if (m == "update" && config.getBool("parallelUpdate", true)) {
			list jobs;
			for (auto it = comps.begin(); it != comps.end(); ++it) {
				auto comp = it->getObject<component>();
				if (!comp || gatedByDisabledEntity(comp)) continue;
				// func preserves dynamic dispatch through the prototype
				// chain; the promise prepends self, which we ignore.
				auto f = func([comp](list) mutable -> var {
					return comp.callMethod("update");
				});
				jobs.pushObject(promise(comp, f, args));
			}
			awaitList(jobs);
			return;
		}
		for (auto it = comps.begin(); it != comps.end(); ++it) {
			auto comp = it->getObject<component>();
			if (!comp) continue;
			if (gatedByDisabledEntity(comp)) continue;
			comp.callMethod(m, args);
		}
	}

	list engine::findAll(object proto) {
		auto ret = list({});
		auto comps = getList("components");
		for (auto it = comps.begin(); it != comps.end(); ++it)
			if (it->isObject(proto)) ret.pushVar(*it);
		return ret;
	}

	void engine::drawScene() {
		// The render dispatch. Sprites draw with just the view id their
		// prototype carries; mesh renderers draw with the PBR scene
		// bundle (view, camera, lights, environment, occlusion). This
		// used to collect the scene and stop, and the loop's bare
		// callMethod("draw") passed no args, which renderables reject —
		// nothing ever rendered.
		auto renderables = findAll(renderable::getPrototype());
		if (renderables.size() == 0) return;

		auto cameras = getList("cameras");
		auto cam = cameras.size() > 0
					   ? cameras.getVar(0).getObject<camera>()
					   : camera();
		auto lights = findAll(light::getPrototype());
		auto envs = findAll(envMap::getPrototype());
		auto env = envs.size() > 0
					   ? envs.getVar(0).getObject<envMap>()
					   : envMap();
		auto occ = occlusionQuery();

		for (auto it = renderables.begin(); it != renderables.end();
				 ++it) {
			auto comp = it->getObject<renderable>();
			// Disabled entities' renderables do not draw.
			if (gatedByDisabledEntity(comp)) continue;
			auto meshR = it->getObject<meshRenderer>();
			if ((bool)meshR) {
				auto drawArgs = list();
				drawArgs.pushVar(
					var(comp.getList("view").getUInt16(0)));
				drawArgs.pushObject(cam);
				drawArgs.pushVar(var(lights));
				drawArgs.pushObject(env);
				drawArgs.pushObject(occ);
				meshR.draw(drawArgs);
			} else
				comp.callMethod("draw", comp.getList("view"));
		}
	}

	var engine::start(list) {
		auto win = getObject<window>("window");
		auto gfx = getObject<gfxBackend>("graphics");
		auto phys = getObject<world>("world");
		gfx.setObject("window", win);

		// The MCP debug surface (config "mcp" -> the loaded seam): tool
		// calls queue on the endpoint and drain HERE — a safe point
		// (nothing mid-frame is half-mutated, the previous frame has
		// fully rendered). Cheap when the queue is empty.
		auto* mcpSystem = (mcpServerSystem*)getPtr("mcpSystem");

		auto cameras = getList("cameras");
		auto comps = getList("components");

		setBool("running", true);
		auto ws = win.getBackend();

		// Frame-rate cap (config "frameTime", ms). bgfx::frame() already
		// throttles to vsync when a compositor presents; this keeps the loop
		// from pegging the CPU when it does not.
		auto config = getObject("config");
		auto frameTime = config.getFloat("frameTime", 16.0);
		using clock = std::chrono::steady_clock;
		auto frameInterval =
			std::chrono::duration<double, std::milli>(frameTime);
		auto last = clock::now();

		// A debugging aid: config {"screenshot", path} requests one PNG
		// at frame 16 (the early frames finish their setup by then);
		// useful for CI and agents checking a render.
		const auto shotPath = config.getString("screenshot", string());
		const bool wantShot = !shotPath.empty();
		bool shotTaken = false;
		uint64_t frameCount = 0;

		while (getBool("running")) {
			if (mcpSystem) mcpSystem->pump();
			if (ws) {
				object ev;
				while (ws->poll(ev)) {
					if (ev.getString("type") == "quit")
						setBool("running", false);
					win.handleEvent({ev});
				}
			}
			phys.step();
			gfx.preFrame();
			for (auto it = cameras.begin(); it != cameras.end();
					 ++it) {
				auto c = it->getObject<camera>();
				c.setView();
			}
			sortComponents();
			initComps();
			sortComponents();
			callMethod("update");
			// The render dispatch: drawScene gives every renderable its
			// proto's view ids (the old bare callMethod("draw") passed no
			// args, which renderables reject, so nothing ever rendered).
			drawScene();
			phys.debugDraw();
			gfx.renderFrame();

			++frameCount;
			if (wantShot && !shotTaken && frameCount >= 16) {
				shotTaken = true;
				gfx.screenshot(ja(shotPath));
			}

			auto now = clock::now();
			auto elapsed = now - last;
			if (elapsed < frameInterval)
				std::this_thread::sleep_for(frameInterval - elapsed);
			last = clock::now();
		}
		cleanUp();
		return var();
	}

	void engine::boot(string company, string gameName) {
		setParent(getPrototype());
		setList("entities", list({}));
		setList("cameras", list({}));
		setList("components", list({}));
		setString("company", company);
		setString("gameName", gameName);
	}

	engine::engine() : obj() {}

	engine::engine(string company, string gameName) : obj() {
		boot(company, gameName);
		initialize();
	}

	// The flags must exist before initialize() reads the settings, since
	// the constructor itself boots everything.
	engine::engine(string company, string gameName, int argc, char* argv[])
		: obj() {
		boot(company, gameName);
		if (argv && argc > 0) {
			list tokens;
			for (int i = 0; i < argc; ++i) tokens.pushString(argv[i]);
			auto overrides = backendOverrides(tokens);
			if (overrides) setObject("backendOverrides", overrides);
		}
		initialize();
	}

	object engine::backendOverrides(list args) {
		// One flag per selectable backend; each writes a single key into
		// the settings section it names.
		static const map<string, pair<string, string>> flags = {
			{"window-backend", {"window", "backend"}},
			{"windowBackend", {"window", "backend"}},
			{"render-backend", {"graphics", "renderBackend"}},
			{"renderBackend", {"graphics", "renderBackend"}},
			{"renderer", {"graphics", "backend"}},
			// The MCP debug endpoint: --mcp=on|off|PORT, --mcp-port=PORT.
			{"mcp", {"mcp", "enabled"}},
			{"mcp-port", {"mcp", "port"}},
			{"mcpPort", {"mcp", "port"}},
		};

		object overrides;
		for (uint64_t i = 0; i < args.size(); ++i) {
			auto token = args[i].getString();
			if (!token.starts_with("--")) continue;
			token.erase(0, 2);
			auto cut = token.find('=');
			auto name = cut != string::npos ? token.substr(0, cut)
											: token;
			// Joined values ride the "='; split values take the next
			// token when it is not itself a flag.
			string value;
			if (cut != string::npos) {
				value = token.substr(cut + 1);
			} else if (i + 1 < args.size()) {
				auto next = args[i + 1].getString();
				if (!next.empty() && !next.starts_with("--")) {
					value = next;
					++i;
				}
			}

			auto flag = flags.find(name);
			if (flag == flags.end() || value.empty()) continue;
			auto section =
				overrides.getObject(flag->second.first, obj({}));
			// The MCP section carries typed values: a port number both
			// enables and binds, on/off is a boolean.
			if (flag->second.first == "mcp") {
				if (flag->second.second == "port") {
					section.setBool("enabled", true);
					section.setInt64("port", atoll(value.c_str()));
				} else if (value == "off" || value == "false")
					section.setBool("enabled", false);
				else if (value == "on" || value == "true")
					section.setBool("enabled", true);
				else {
					// A number: enable at that port.
					section.setBool("enabled", true);
					section.setInt64("port", atoll(value.c_str()));
				}
			}
			// The window config takes a fallback chain: commas become
			// the name list the window facade already accepts.
			else if (flag->second.first == "window" &&
					 value.find(',') != string::npos) {
				list names;
				stringstream values(value);
				string part;
				while (getline(values, part, ','))
					if (!part.empty()) names.pushString(part);
				section.setList("backend", names);
			} else
				section.setString(flag->second.second, value);
			overrides.setObject(flag->second.first, section);
		}
		return overrides;
	}

	set<string> engine::allowedConfigNames() {
		return {"window", "graphics", "mcp"};
	}

	engine& engine::operator+=(list items) {
		auto a = getList("entities");
		auto c = getList("cameras");
		auto components = getList("components");
		if (!a) return *this;
		function<void(entity&)> forEntity = [&,
																				 *this](entity& ent) {
			if (!ent) return;
			if (a.find(ent) == a.end()) a.pushObject(ent);
			ent.setObject("engine", *this);
			auto children = ent.getList("children");
			for (auto cit = children.begin(); cit != children.end();
					 ++cit) {
				auto child = cit->getObject<entity>();
				forEntity(child);
			}
			auto comps = ent.getList("components");
			for (auto cit = comps.begin(); cit != comps.end();
					 ++cit) {
				auto comp = cit->getObject<component>();
				if (comp && components.find(comp) == components.end())
					components.pushObject(comp);
			}
		};
		for (auto it = items.begin(); it != items.end(); ++it) {
			if (it->isObject(camera::getPrototype()))
				c += {*it};
			else if (it->isObject(entity::getPrototype())) {
				auto ent = it->getObject<entity>();
				forEntity(ent);
			}
		}
		return *this;
	}

	engine& engine::operator-=(list items) {
		auto a = getList("entities");
		auto c = getList("cameras");
		auto components = getList("components");
		if (!a) return *this;
		function<void(var&)> forComp = [&](var& compVar) {
			auto comp = compVar.getObject<component>();
			if (comp) {
				comp.callMethod("destroy");
			}
			auto it = components.find(compVar);
			if (it != components.end()) components.erase(it);
		};
		function<void(var&)> forEntity = [&](var& entVar) {
			auto ent = entVar.getObject<entity>();
			if (ent) {
				auto children = ent.getList("children");
				for (auto cit = children.begin(); cit != children.end();
						 ++cit)
					forEntity(*cit);
				auto comps = ent.getList("components");
				for (auto cit = comps.begin(); cit != comps.end();
						 ++cit) {
					forComp(*cit);
				}
				ent.callMethod("destroy");
				ent.erase("engine");
			}
			auto it = a.find(entVar);
			if (it != a.end()) a.erase(it);
		};

		while (items.size() > 0) {
			auto it = items.begin();
			if (it->isObject(entity::getPrototype())) {
				forEntity(*it);
			} else if (it->isObject(component::getPrototype())) {
				forComp(*it);
			}
		}
		return *this;
	}

	void engine::cleanUp() {
		// Stop the MCP endpoint before anything it can reach is torn
		// down: tool calls (screenshots, method calls) must not run
		// against dying backends.
		if (auto* mcpSystem = (mcpServerSystem*)getPtr("mcpSystem")) {
			mcpSystem->shutdown();
			erase("mcpSystem");
			delete mcpSystem;
		}

		auto win = getObject<window>("window");
		auto gfx = getObject<gfxBackend>("graphics");
		auto phys = getObject<world>("world");

		saveSettings();

		auto a = getList("entities");
		auto c = getList("cameras");
		auto components = getList("components");

		function<void(var&)> forComp = [&](var& compVar) {
			auto comp = compVar.getObject<component>();
			if (comp) {
				comp.callMethod("destroy");
			}
			auto it = components.find(compVar);
			if (it != components.end()) components.erase(it);
		};
		function<void(var&)> forEntity = [&](var& entVar) {
			auto ent = entVar.getObject<entity>();
			if (ent) {
				auto children = ent.getList("children");
				for (auto cit = children.begin(); cit != children.end();
						 ++cit)
					forEntity(*cit);
				auto comps = ent.getList("components");
				for (auto cit = comps.begin(); cit != comps.end();
						 ++cit) {
					forComp(*cit);
				}
				ent.callMethod("destroy");
				ent.erase("engine");
			}
			auto it = a.find(entVar);
			if (it != a.end()) a.erase(it);
		};

		while (components.size() > 0) {
			auto it = components.begin();
			if (it->isObject(entity::getPrototype())) {
				forEntity(*it);
			} else if (it->isObject(component::getPrototype())) {
				forComp(*it);
			}
		}

		gfx.destroy();
		win.destroy();
		empty();
	}

}  // namespace gold