#include <iostream>
#include <filesystem>
#include <fstream>

#include "game/inputSystem.hpp"
#include "game/window.hpp"
#include "game/windowSystem.hpp"
#include "game/boxShape.hpp"
#include "game/light.hpp"
#include "game/physicsBackend.hpp"
#include "game/physicsBody.hpp"
#include "game/sphereShape.hpp"
#include "game/sprite.hpp"
#include "game/camera.hpp"
#include "game/transform.hpp"
#include "game/world.hpp"
#include "game/graphics.hpp"
#include "game/mesh.hpp"
#include "game/meshRenderer.hpp"
#include "game/uiSurface.hpp"
#include "game/engine.hpp"
#include "image.hpp"
#include "goldjs.hpp"
#include "goldtest.hpp"
#include "plugin.hpp"
#include "promise.hpp"

using namespace gold;

// Expose the protected component-dispatch path for testing.
struct testEngine : public engine {
	using engine::callMethod;
};

TEST(window_backend_fallback) {
	// Explicit headless.
	auto h = createWindowSystem("headless");
	EXPECT_TRUE(h != nullptr);
	if (h) {
		EXPECT_EQ(string(h->name()), string("headless"));
		delete h;
	}

	// Fallback chain: a name that doesn't exist -> headless.
	auto fallback =
		createWindowSystem(list({var(string("no_such_backend")),
			var(string("headless"))}));
	EXPECT_TRUE(fallback != nullptr);
	if (fallback) {
		EXPECT_EQ(string(fallback->name()), string("headless"));
		delete fallback;
	}

	// Entirely unknown chain yields nothing.
	auto none = createWindowSystem(list({var(string("nope_a")),
		var(string("nope_b"))}));
	EXPECT_TRUE(none == nullptr);
}

TEST(backend_plugin_loads_and_registers) {
	// The fixture plugin is a genuine shared library (libgoldFixtureWindow)
	// built next to this executable: createWindowSystem's hook finds it on
	// the first miss and gets a registered backend back.
	auto* sys = createWindowSystem("fixtureWindow");
	EXPECT_TRUE(sys != nullptr);
	if (sys) {
		EXPECT_EQ(string(sys->name()), string("fixtureWindow"));
		delete sys;
	}

	// The fallback chain reaches plugin-loaded backends too.
	auto* chained = createWindowSystem(
		list({var(string("no_such_backend")), var(string("fixtureWindow"))}));
	EXPECT_TRUE(chained != nullptr);
	if (chained) delete chained;

	// A genuine miss stays a miss, and the loader explains itself.
	EXPECT_FALSE(plugin::load("NoSuchBackendAnywhere"));
	EXPECT_FALSE(plugin::lastError().empty());
}

TEST(uisurface_selects_renderer_through_registry) {
	// Construction resolves the backend through the renderer registry
	// (config "renderer" picks it in initialize()); without a GPU backend
	// nothing else happens headlessly.
	uiSurface* s = new uiSurface();
	EXPECT_EQ(s->getString("renderer"), string("software"));
	delete s;
}

TEST(window_backend_loads_from_the_sdl_plugin) {
	// The SDL3 backends ship in libgoldSDL3 (built when the system SDL3
	// dev package exists): the registry hook loads the plugin on a miss.
	if (!plugin::probe("sdl3")) return;  // no SDL3 on this machine

	auto* sys = createWindowSystem("sdl");
	EXPECT_TRUE(sys != nullptr);
	if (sys) {
		EXPECT_EQ(string(sys->name()), string("sdl"));
		delete sys;
	}

	auto* input = createInputSystem("sdl");
	EXPECT_TRUE(input != nullptr);
	if (input) delete input;
}

TEST(window_backend_direct_sdl2_selection) {
	// The SDL2 parity adapter (built when the system sdl2 headers exist)
	// is selectable by its concrete name, and the alias chain prefers it
	// only when the SDL3 plugin is absent.
	if (!plugin::probe("sdl2") || !plugin::load("sdl2")) return;

	auto* sys = createWindowSystem("sdl2");
	EXPECT_TRUE(sys != nullptr);
	if (sys) {
		EXPECT_EQ(string(sys->name()), string("sdl2"));
		delete sys;
	}

	// The generic "sdl" alias still prefers the SDL3 plugin when both
	// exist; on this machine both are present, so it resolves to sdl.
	auto* alias = createWindowSystem("sdl");
	EXPECT_TRUE(alias != nullptr);
	if (alias) delete alias;
}

TEST(window_gold_events) {
	// Headless backend never produces events.
	auto sys = createWindowSystem("headless");
	object ev;
	EXPECT_FALSE(sys->poll(ev));
	EXPECT_EQ(ev.size(), uint64_t(0));
	delete sys;

	// The window facade dispatches gold-object events through its on*
	// handler slots, updating state with the defaults.
	auto win = window(jo("backend", "headless"));
	auto err = win.create();
	EXPECT_TRUE(err.isEmpty());
	EXPECT_EQ(string(win.getString("backend")), string("headless"));

	win.handleEvent({jo("type", "resized", "width", 1024, "height", 768)});
	EXPECT_EQ(win.getInt32("width"), 1024);
	EXPECT_EQ(win.getInt32("height"), 768);

	win.handleEvent({jo("type", "moved", "x", 10, "y", 20)});
	EXPECT_EQ(win.getInt32("x"), 10);
	EXPECT_EQ(win.getInt32("y"), 20);

	win.handleEvent({jo("type", "focus_gained")});
	EXPECT_TRUE(win.getBool("active"));

	win.handleEvent({jo("type", "hidden")});
	EXPECT_TRUE(win.getBool("hidden"));

	win.handleEvent({jo("type", "key_down", "keyCode", 42)});
	EXPECT_EQ(win.getInt32("keyCode"), 42);

	win.handleEvent({jo("type", "mouse_wheel", "scrollY", -3)});
	EXPECT_EQ(win.getInt32("scrollY"), -3);

	// onQuit flags the window; unknown event types are ignored.
	win.handleEvent({jo("type", "quit")});
	EXPECT_TRUE(win.getBool("quit"));
	win.handleEvent({jo("type", "completely_made_up")});
	EXPECT_TRUE(win.getBool("quit"));

	win.destroy();
}

TEST(window_handler_override) {
	auto win = window(jo("backend", "headless"));
	auto err = win.create();
	EXPECT_TRUE(err.isEmpty());

	// Custom handler overrides the prototype default.
	int resizedCount = 0;
	auto hResize = func([&](list args) -> var {
		resizedCount++;
		EXPECT_EQ(args[1].getObject().getInt32("width"), 320);
		return var();
	});
	win.setFunc("onResized", hResize);
	win.handleEvent({jo("type", "resized", "width", 320)});
	EXPECT_EQ(resizedCount, 1);

	// The override replaced the default: window state is NOT updated.
	EXPECT_NE(win.getInt32("width"), 320);

	win.destroy();
}

TEST(input_backend_interface) {
	auto input = createInputSystem("evdev");
	EXPECT_TRUE(input != nullptr);
	if (!input) return;
	EXPECT_EQ(string(input->name()), string("evdev"));

	// No devices opened: poll produces nothing.
	object ev;
	EXPECT_FALSE(input->poll(ev));

	// Nonexistent device paths fail gracefully.
	EXPECT_FALSE(input->open({"/dev/input/does-not-exist"}));

	// Unknown backend name yields null.
	auto bogus = createInputSystem("no_such_input");
	EXPECT_TRUE(bogus == nullptr);

	delete input;
}

TEST(image_invalid_operations_are_errors) {
	image empty;
	EXPECT_TRUE(empty.convert(list()).isError());
	EXPECT_TRUE(empty.convert({var(uint32_t(1))}).isError());
	EXPECT_TRUE(empty.toLinearRGBA32F().isError());
	EXPECT_TRUE(empty.getRawData(list()).isError());
}

TEST(window_setters_accept_empty_arguments) {
	window win;
	win.setSize(list());
	win.setPos(list());
	win.setTitle(list());
	win.setFullscreen(list());
	win.setBorderless(list());
	EXPECT_EQ(win.getInt32("width"), WindowCentered);
	EXPECT_EQ(win.getInt32("height"), WindowCentered);
	EXPECT_FALSE(win.getBool("fullscreen"));
	EXPECT_FALSE(win.getBool("borderless"));
}

TEST(game_setters_report_missing_arguments) {
	sprite spr;
	light lamp;
	EXPECT_TRUE(spr.setArea(list()).isError());
	EXPECT_TRUE(lamp.setIntensity(list()).isError());
	EXPECT_TRUE(lamp.setType(list()).isError());
}

TEST(camera_and_transform_setters_report_missing_arguments) {
	camera cam;
	transform trans;
	EXPECT_TRUE(cam.setViewSize(list()).isError());
	EXPECT_TRUE(cam.setViewOffset(list()).isError());
	EXPECT_TRUE(trans.setPosition(list()).isError());
	EXPECT_TRUE(trans.setRotation(list()).isError());
	EXPECT_TRUE(trans.setScale(list()).isError());
}

TEST(world_debug_draw_reports_uninitialized_world) {
	world scene;
	EXPECT_TRUE(scene.debugDraw().isError());
}

TEST(gltf_external_buffers_and_normalized_accessors) {
	auto root = std::filesystem::temp_directory_path() / "gold_gltf_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);
	std::filesystem::create_directories(root);
	{
		std::ofstream bin(root / "data.bin", std::ios::binary);
		const unsigned char bytes[] = {
			0x78, 0x56, 0x34, 0x12, 0, 128, 255, 9, 8, 7,
			128, 0, 127, 9, 9, 9, 9, 127, 128, 0, 8, 8, 8, 8,
			128, 0, 127, 7, 7, 7, 7, 127, 128, 0, 6, 6, 6, 6};
		bin.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
	}
	std::ofstream gltf(root / "scene.gltf");
	gltf << R"({"buffers":[{"uri":"data.bin","byteLength":38}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":7},{"buffer":0,"byteOffset":7,"byteLength":3},{"buffer":0,"byteOffset":10,"byteLength":28,"byteStride":16}],"images":[{"uri":"data.bin"},{"uri":"data:application/octet-stream;base64,AAECAw=="},{"bufferView":1,"mimeType":"application/octet-stream"}],"samplers":[{"wrapS":33071,"wrapT":33648}],"textures":[{"source":0,"sampler":0}],"materials":[{"normalTexture":{"index":0},"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"accessors":[{"bufferView":0,"componentType":5125,"count":1,"type":"SCALAR"},{"bufferView":0,"byteOffset":4,"componentType":5121,"count":1,"type":"VEC3","normalized":true},{"bufferView":2,"componentType":5120,"count":2,"type":"VEC3","normalized":true}]})";
	gltf.close();

	mesh loaded(root / "scene.gltf");
	EXPECT_TRUE(mesh::assetCacheSize() >= (uint64_t)1);
	EXPECT_EQ(loaded.getString("error"), "");
	auto accessors = loaded.getList("accessors");
	EXPECT_EQ(accessors.getObject(0).getList("parsed").getUInt32(0), 0x12345678u);
	auto color = accessors.getObject(1).getList("parsed").getVar(0);
	EXPECT_NEAR(color.getFloat(0), 0.0f, 1e-6);
	EXPECT_NEAR(color.getFloat(1), 128.0f / 255.0f, 1e-6);
	EXPECT_NEAR(color.getFloat(2), 1.0f, 1e-6);
	EXPECT_EQ(accessors.getObject(1).getList("raw").getVar(0).getUInt8(1), 128u);
	EXPECT_EQ(loaded.getList("images").getObject(0).getBinary("data").size(), (size_t)38);
	EXPECT_EQ(loaded.getList("images").getObject(1).getBinary("data").size(), (size_t)4);
	EXPECT_EQ(loaded.getList("images").getObject(2).getBinary("data").size(), (size_t)3);
	auto texture = loaded.getList("textures").getObject(0);
	EXPECT_EQ(texture.getObject("image").getBinary("data").size(), (size_t)38);
	EXPECT_EQ(texture.getObject("sampler").getUInt32("wrapS"), 33071u);
	EXPECT_TRUE(loaded.getList("materials").getObject(0)
		.getObject("pbrMetallicRoughness").getObject("baseColorTexture")
		.getObject("resolvedTexture"));
	EXPECT_TRUE(loaded.getList("materials").getObject(0)
		.getObject("normalTexture").getObject("resolvedTexture"));
	auto interleaved = accessors.getObject(2).getList("parsed");
	EXPECT_EQ(interleaved.size(), (uint64_t)2);
	EXPECT_NEAR(interleaved.getVar(0).getFloat(0), -1.0f, 1e-6);
	EXPECT_NEAR(interleaved.getVar(0).getFloat(2), 1.0f, 1e-6);
	mesh layoutFixture;
	layoutFixture.setList("accessors", accessors);
	layoutFixture.setList("nodes", list({jo("name", "node", "mesh", uint64_t(0))}));
	layoutFixture.setList("meshes", list({jo("primitives", list({jo("attributes", jo("POSITION", uint64_t(2), "COLOR_0", uint64_t(1)))}))}));
	auto layout = layoutFixture.getVertexLayoutHandle({"node", uint64_t(0)})
		.getObject<vertexLayout>();
	EXPECT_TRUE(layout);
	EXPECT_EQ(layout.getType("attributes"), typeList);
	EXPECT_TRUE(layout.getList("attributes").size() == (uint64_t)2);
	EXPECT_TRUE(layout.getList("attributes").getObject(1).getBool("normalized"));

	std::ofstream missing(root / "missing.gltf");
	missing << R"({"buffers":[{"uri":"no.bin","byteLength":1}]})";
	missing.close();
	mesh invalid(root / "missing.gltf");
	EXPECT_TRUE(invalid.getString("error").find("external glTF buffer") != string::npos);
	std::ofstream badRefs(root / "bad_refs.gltf");
	badRefs << R"({"images":[{"uri":"data:application/octet-stream;base64,AA=="}],"textures":[{"source":1}]})";
	badRefs.close();
	mesh bad(root / "bad_refs.gltf");
	EXPECT_TRUE(bad.getString("error").find("image reference") != string::npos);
	std::filesystem::remove_all(root, ec);
}

TEST(gltf_asset_cache_can_be_cleared) {
	mesh::clearAssetCache();
	EXPECT_EQ(mesh::assetCacheSize(), (uint64_t)0);
}

TEST(gpu_texture_cleanup_is_idempotent) {
	gpuTexture texture;
	texture.destroy();
	texture.destroy();
	EXPECT_EQ(texture.getUInt16("idx"), bgfx::kInvalidHandle);
}

TEST(headless_graphics_lifecycle) {
	gfxBackend gfx;
	EXPECT_TRUE(gfx.initialize(list()).isError());
	auto win = window(jo("backend", "headless"));
	EXPECT_FALSE(win.create().isError());
	EXPECT_FALSE(gfx.initialize({win}).isError());
	EXPECT_FALSE(gfx.preFrame().isError());
	EXPECT_FALSE(gfx.renderFrame().isError());
	EXPECT_FALSE(gfx.destroy().isError());
	EXPECT_FALSE(gfx.destroy().isError());
	win.destroy();
}

TEST(public_game_entrypoints_report_invalid_calls) {
	window win;
	EXPECT_TRUE(win.handleEvent(list()).isError());
	EXPECT_TRUE(win.handleEvent({obj()}).isError());
	mesh model;
	EXPECT_TRUE(model.getMaterial(list()).isError());
	EXPECT_TRUE(model.getMaterialFromPrimitive(list()).isError());
	sprite spr;
	EXPECT_TRUE(spr.draw(list()).isError());
	meshRenderer renderer;
	EXPECT_TRUE(renderer.draw(list()).isError());
}

TEST(glb_embedded_json_and_binary_chunks) {
	auto root = std::filesystem::temp_directory_path() / "gold_glb_test";
	std::error_code ec;
	std::filesystem::remove_all(root, ec);
	std::filesystem::create_directories(root);
	auto json = std::string(
		R"({"buffers":[{"byteLength":4}],"bufferViews":[{"buffer":0,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5125,"count":1,"type":"SCALAR"}]})");
	while (json.size() % 4 != 0) json.push_back(' ');
	std::vector<uint8_t> glb;
	auto put32 = [&glb](uint32_t value) {
		for (int i = 0; i < 4; ++i) glb.push_back(uint8_t(value >> (i * 8)));
	};
	put32(0x46546C67);
	put32(2);
	put32(uint32_t(12 + 8 + json.size() + 8 + 4));
	put32(uint32_t(json.size()));
	put32(0x4E4F534A);
	glb.insert(glb.end(), json.begin(), json.end());
	put32(4);
	put32(0x004E4942);
	put32(0x12345678);
	std::ofstream output(root / "scene.glb", std::ios::binary);
	output.write(reinterpret_cast<const char*>(glb.data()), glb.size());
	output.close();

	mesh loaded(root / "scene.glb");
	EXPECT_EQ(loaded.getString("error"), "");
	EXPECT_EQ(loaded.getList("accessors").getObject(0).getList("parsed")
		.getUInt32(0), 0x12345678u);

	std::ofstream bad(root / "bad.glb", std::ios::binary);
	bad.write("bad", 3);
	bad.close();
	mesh invalid(root / "bad.glb");
	EXPECT_TRUE(invalid.getString("error").find("GLB") != string::npos);
	std::filesystem::remove_all(root, ec);
}

TEST(engine_parallel_component_update) {
	// Parallel component updates are on by default (config "parallelUpdate"
	// defaults to true). With the promise pool active, component updates run
	// on worker threads; each component's "update" func slot is invoked and
	// a shared counter must reach the component count.
	static int count = 0;
	count = 0;
	auto bump = func([](list) -> var {
		count++;
		return var();
	});

	testEngine eng;
	// No explicit config: parallelUpdate defaults to true.

	auto comps = list();
	for (int i = 0; i < 4; ++i) {
		component c;
		c.setFunc("update", bump);
		comps.pushObject(c);
	}
	eng.setList("components", comps);

	promise::useAllCores();
	eng.callMethod("update");
	promise::joinThreads();

	EXPECT_EQ(count, 4);
}

// The pointer mapping of a UI surface: a world ray is intersected with the
// quad's plane and turned into a point. Everything downstream (hover, click,
// :active) is driven by this one step, so it is worth pinning down.
TEST(ui_surface_ray_hits_the_screen_quad) {
	float x = 0.0f, y = 0.0f;
	// Straight down the -Z axis from in front of the quad.
	EXPECT_TRUE(intersectQuad(0.5f, 0.25f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, x, y));
	EXPECT_NEAR(x, 0.5, 0.0001);
	EXPECT_NEAR(y, 0.25, 0.0001);

	// An angled ray still lands on the plane at the right place.
	EXPECT_TRUE(intersectQuad(0.0f, 0.0f, 2.0f, 1.0f, 0.5f, -1.0f, 0.0f, x, y));
	EXPECT_NEAR(x, 2.0, 0.0001);
	EXPECT_NEAR(y, 1.0, 0.0001);

	// Parallel to the quad: no hit.
	EXPECT_FALSE(intersectQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, x, y));
	// Pointing away from it: no hit.
	EXPECT_FALSE(intersectQuad(0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, x, y));
}

TEST(physics_backend_registry) {
	// The built-in no-op "none" backend keeps the facades alive on
	// machines without a physics engine, with the usual chain + miss
	// semantics.
	auto* none = createPhysicsBackend("none");
	EXPECT_TRUE(none != nullptr);
	if (none) {
		EXPECT_EQ(string(none->name()), string("none"));
		delete none;
	}
	auto* chained =
		createPhysicsBackend(list({"no_such_backend", "none"}));
	EXPECT_TRUE(chained != nullptr);
	if (chained) delete chained;

	// The bullet backend ships as libgoldBullet; the registry hook loads
	// it on demand when the system bullet package exists.
	auto* bullet = createPhysicsBackend("bullet");
	if (bullet) EXPECT_EQ(string(bullet->name()), string("bullet"));
	if (bullet) delete bullet;

	EXPECT_TRUE(createPhysicsBackend("no_physics_anywhere") == nullptr);
}

TEST(physics_bullet_box_falls_under_gravity) {
	// Gated on the bullet plugin; everything else is the real facade flow
	// (entity + transform + shape descriptor + body) with the engine ref
	// set manually, since no engine lifecycle boots here.
	if (!plugin::load("bullet")) return;
	engine eng;
	// The bare engine facade skips its prototype (inert by convention);
	// the world only needs the prototype-tagged ref to find it.
	eng.setParent(engine::getPrototype());
	world phys(jo("gravity", vec3f(0, -10, 0)));
	auto err = phys.initialize({eng});
	EXPECT_FALSE(err.isError());
	if (err.isError()) return;
	// The engine normally binds the world in its own initialize().
	eng.setObject("world", phys);

	// The config ctor attaches the prototype (the bare ctor is the inert
	// convention) and runs the entity's own initialize().
	entity box(jo("name", "box"));
	box.setObject("engine", eng);
	auto trans = box.getTransform();
	trans.setPosition({vec3f(0, 10, 0)});
	auto shapeComp = boxShape(jo("size", vec3f(1, 1, 1)));
	physicsBody body(jo("mass", 1.0));
	box.add(ja(shapeComp, body));

	// Initialize the body through its facade (in production the engine's
	// initComps dispatches this in priority order). The chain the
	// facade walks: body -> entity -> engine -> world -> backend.
	EXPECT_TRUE((bool)body.getObject<entity>("object"));
	EXPECT_TRUE((bool)body.getObject<entity>("object").getObject<engine>("engine"));
	EXPECT_TRUE((bool)eng.getObject<world>("world"));
	EXPECT_TRUE(body.callMethod("initialize").isEmpty());
	EXPECT_NE(body.getPtr("body"), (void*)nullptr);
	EXPECT_EQ(phys.getList("bodies").size(), (uint64_t)1);

	const float startY = trans.getPosition().getFloat(1);
	for (int i = 0; i < 30; ++i)
		phys.step(list({1.0 / 60.0}));
	const float endY = trans.getPosition().getFloat(1);
	// It fell from 10 and kept falling (transform synced from the sim).
	EXPECT_TRUE(endY < startY);
	EXPECT_TRUE(endY < 1.0f);

	// The backend explains itself through the world's registry entry.
	auto backend = (physicsBackend*)phys.getPtr("physicsBackend");
	EXPECT_NE(backend, (physicsBackend*)nullptr);
}

TEST(entity_disable_gates_update_dispatch) {
	// A disabled entity skips its components on dispatch; enabling
	// resumes them. The parent flag cascades down the chain; loose
	// components (no owning entity) always dispatch.
	static int updateCount = 0;
	updateCount = 0;
	auto bump = func([](list) -> var {
		++updateCount;
		return var();
	});

	testEngine eng;
	// The sequential dispatch path (an earlier test in this process
	// leaves the parallel pool running; a config pins the sync path).
	eng.setObject("config", jo("parallelUpdate", false));

	// An owned component on an entity, a loose component, and a child
	// entity nested under a disabled parent.
	auto ent = entity(jo("name", "gated"));
	auto comp = component();
	comp.setFunc("update", bump);
	ent.add(ja(comp));

	auto loose = component();
	loose.setFunc("update", bump);

	auto parent = entity(jo("name", "parent"));
	auto child = entity(jo("name", "child"));
	auto childComp = component();
	childComp.setFunc("update", bump);
	child.add(ja(childComp));
	parent.add(ja(child));

	eng.setList("components", ja(var(comp), var(loose), var(childComp)));

	eng.callMethod("update");
	EXPECT_EQ(updateCount, 3);

	// The disable gates exactly the owned component's dispatch.
	ent.disable({});
	eng.callMethod("update");
	EXPECT_EQ(updateCount, 5);

	// The disabled parent gates the child too (the owned one is still
	// gated by its own flag).
	parent.disable({});
	eng.callMethod("update");
	EXPECT_EQ(updateCount, 6);

	// Re-enabling resumes, and the chain stays untouched otherwise.
	parent.enable({});
	ent.enable({});
	eng.callMethod("update");
	EXPECT_EQ(updateCount, 9);
}

TEST(physics_raycast_hits_box) {
	// Gated on the bullet plugin; the ray cast is answered by the
	// backend and reports the struck body + hit location.
	if (!plugin::load("bullet")) return;
	engine eng;
	eng.setParent(engine::getPrototype());
	world phys(jo("physics", "bullet", "gravity", vec3f(0, -10, 0)));
	EXPECT_FALSE(phys.initialize({eng}).isError());
	eng.setObject("world", phys);

	// A static box (mass 0): boxShape's "size" feeds btBoxShape as
	// half-extents, so the box spans +-1 per axis and a ray straight
	// down from y=5 crosses the top face at y=+1.
	auto bodyComp = physicsBody(jo("mass", 0.0));
	entity ground(jo("name", "ground"));
	ground.setObject("engine", eng);
	ground.getTransform().setPosition({vec3f(0, 0, 0)});
	ground.add(ja(boxShape(jo("size", vec3f(1, 1, 1))), bodyComp));
	EXPECT_TRUE(bodyComp.callMethod("initialize").isEmpty());

	auto hit = phys.raytrace({ja(0, 5, 0), ja(0, -5, 0)}).getList();
	EXPECT_EQ(hit.size(), (uint64_t)1);
	if (hit.size() == 0) return;
	auto first = hit.getObject(0);
	EXPECT_NEAR(first.getVar("position").getFloat(1), 1.0f, 0.001f);
	EXPECT_NEAR(first.getVar("normal").getFloat(1), 1.0f, 0.001f);
	EXPECT_TRUE(first.getDouble("distance") > 3.0f);
	EXPECT_TRUE((bool)first.getObject("body"));

	// A miss far away answers an empty list.
	auto miss = phys.raytrace({ja(0, 5, 100), ja(0, -5, 100)}).getList();
	EXPECT_EQ(miss.size(), (uint64_t)0);
}

TEST(varying_define_expansion) {
	// The system shaderc rejects preprocessor directives in varying
	// definitions, so the runtime compile expands them itself. Feed a
	// mini varying.def through expandVaryingDefinition and check the
	// directive-free result per define set.
	namespace fs = std::filesystem;
	auto root = fs::temp_directory_path() / "gold_varying_test";
	std::error_code ec;
	fs::create_directories(root, ec);
	auto defPath = (root / "test.def.sc").string();
	{
		std::ofstream out(defPath);
		out << "vec3 v_position:POSITION = vec3(0.0, 0.0, 0.0);\n"
		       "#if defined(HAS_NORMALS) && !defined(HAS_TANGENTS)\n"
		       "vec3 v_normal:NORMAL = vec3(0.0, 0.0, 0.0);\n"
		       "#endif\n"
		       "#if defined(HAS_NORMALS) && defined(HAS_TANGENTS)\n"
		       "vec3 v_tbn0:TEXCOORD2 = vec3(0.0, 0.0, 0.0);\n"
		       "#endif\n"
		       "vec3 a_position:POSITION;\n"
		       "#ifdef HAS_NORMALS\n"
		       "vec3 a_normal:NORMAL;\n"
		       "#endif\n"
		       "vec2 a_texcoord0:TEXCOORD0;\n";
	}

	auto read = [](const string& p) {
		std::ifstream in(p);
		return string(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());
	};

	// With normals: v_normal + a_normal active, tbn gone.
	auto expanded = read(expandVaryingDefinition(defPath, "HAS_NORMALS=1;"));
	EXPECT_TRUE(expanded.find("v_normal") != string::npos);
	EXPECT_TRUE(expanded.find("a_normal") != string::npos);
	EXPECT_TRUE(expanded.find("v_tbn0") == string::npos);
	EXPECT_TRUE(expanded.find('#') == string::npos);

	// With normals and tangents: tbn in, v_normal out.
	expanded =
		read(expandVaryingDefinition(defPath, "HAS_NORMALS=1;HAS_TANGENTS=1;"));
	EXPECT_TRUE(expanded.find("v_tbn0") != string::npos);
	EXPECT_TRUE(expanded.find("a_normal") != string::npos);
	EXPECT_TRUE(expanded.find("v_normal") == string::npos);

	// Bare: only the unconditional declarations survive.
	expanded = read(expandVaryingDefinition(defPath, ""));
	EXPECT_TRUE(expanded.find("a_position") != string::npos);
	EXPECT_TRUE(expanded.find("a_texcoord0") != string::npos);
	EXPECT_TRUE(expanded.find("a_normal") == string::npos);
	EXPECT_TRUE(expanded.find("v_normal") == string::npos);

	fs::remove_all(root, ec);
}

TEST(transform_trs_composes) {
	// Regression: getMatrix used to build its TRS via var operators
	// that each REPLACED the matrix (translate/scale/rotate overwrite),
	// so only the translation survived and rotations silently vanished.
	transform t(jo());
	t.setPosition({1.0, 2.0, 3.0});
	t.setAxisRotation({vec3f(0, 1, 0), 1.5707963267948966});
	t.setScale({2.0, 2.0, 2.0});
	auto m = t.getMatrix();
	// Expected row-major: scale 2, then 90&deg; yaw, then translate:
	//  [ 0 0 2 0; 0 2 0 0; -2 0 0 0; 1 2 3 1 ]
	EXPECT_NEAR(m.getFloat(12), 1.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(13), 2.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(14), 3.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(0), 0.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(2), 2.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(8), -2.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(5), 2.0f, 0.0001f);
	EXPECT_NEAR(m.getFloat(15), 1.0f, 0.0001f);
}

int main() {
	return goldtest::runAll();
}
