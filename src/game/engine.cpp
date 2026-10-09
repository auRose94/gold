
#include "engine.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "camera.hpp"
#include "component.hpp"
#include "entity.hpp"
#include "envMap.hpp"
#include "game/windowSystem.hpp"
#include "goldjs.hpp"
#include "graphics.hpp"
#include "light.hpp"
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
				if (comp) {
					// func preserves dynamic dispatch through the prototype
					// chain; the promise prepends self, which we ignore.
					auto f = func([comp](list) mutable -> var {
						return comp.callMethod("update");
					});
					jobs.pushObject(promise(comp, f, args));
				}
			}
			awaitList(jobs);
			return;
		}
		for (auto it = comps.begin(); it != comps.end(); ++it) {
			auto comp = it->getObject<component>();
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

	engine::engine() : obj() {}

	engine::engine(string company, string gameName) : obj() {
		setParent(getPrototype());
		setList("entities", list({}));
		setList("cameras", list({}));
		setList("components", list({}));
		setString("company", company);
		setString("gameName", gameName);
		initialize();
	}

	set<string> engine::allowedConfigNames() {
		return {"window", "graphics"};
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