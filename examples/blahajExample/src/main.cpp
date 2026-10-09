#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include <camera.hpp>
#include <engine.hpp>
#include <entity.hpp>
#include <graphics.hpp>
#include <light.hpp>
#include <mesh.hpp>
#include <meshRenderer.hpp>
#include <transform.hpp>
#include <goldjs.hpp>

// After the includes: std must be a defined namespace by then (clang
// rejects the directive against an implicitly-declared namespace).
using namespace std;

using namespace gold;

namespace {
	/** The assets live beside the executable under an "assets" directory
	 *  (the build tree symlinks the source assets there; a bundled
	 *  install ships the same), so run-from-anywhere flows resolve them
	 *  there first and fall back to the working directory. */
	string assetsDir(const char* argv0) {
		namespace fs = std::filesystem;
		if (argv0) {
			auto base = fs::absolute(argv0).parent_path();
			if (fs::is_directory(base / "assets"))
				return (base / "assets").string();
		}
		return "./assets";
	}
}  // namespace

int main(int argc, char** argv) {
	const string assets = assetsDir(argc > 0 ? argv[0] : nullptr);
	const string defaultPath =
		assets + "/models/props/Blahaj/"
		"Blahaj_Low_poly_blahaj1_Low_poly_blahaj1.gltf";
	const string modelPath = argc > 1 ? string(argv[1]) : defaultPath;
	mesh model{path(modelPath)};
	auto error = model.getString("error");
	if (!error.empty()) {
		cerr << "Failed to load " << modelPath << ": " << error << '\n';
		return 1;
	}

	// The glTF inspection report the example always gave.
	cout << "Loaded: " << modelPath << '\n'
		<< "  buffers: " << model.getList("buffers").size() << '\n'
		<< "  accessors: " << model.getList("accessors").size() << '\n'
		<< "  meshes: " << model.getList("meshes").size() << '\n'
		<< "  materials: " << model.getList("materials").size() << '\n'
		<< "  images: " << model.getList("images").size() << '\n';

	// Now render it: the engine loop drives camera views, the PBR mesh
	// dispatch, and the window.
	engine app("GoldRoseCode", "BlahajExample");

	auto cam = app.getPrimaryCamera().getObject<camera>();
	auto camTrans =
		cam.getComponent({transform::getPrototype()})
			.getObject<gold::transform>();
	// The camera looks down +Z from its own transform. The model spans
	// ~0.9 units (glTF-authored), so a close framing: eye at +0.35,
	// 1.6 units back, looking through the origin.
	camTrans.setPosition({0, 0.2, -1.0});

	entity shark(jo("name", "blahaj"));
	auto sharkTrans = shark.getTransform();
	// The spin-and-bob: a component's "update" slot may be a plain gold
	// func — the engine calls it every frame. ~1.4 rad/s yaw (one turn
	// every ~4.5s) with a slow sine bob; both overwrite the transform,
	// so no initial pose is needed.
	auto spin = component(jo(
		"update", func([sharkTrans](list) mutable -> var {
			static double angle = 1.2;
			static double t = 0;
			angle += 0.022;
			t += 0.055;
			sharkTrans.setAxisRotation({vec3f(0, 1, 0), angle});
			sharkTrans.setPosition({0.0, 0.06 * sin(t), 0.0});
			return var();
		})));
	shark += ja(meshRenderer({
		{"mesh", model},
		// Render the whole scene graph: no node name means "from roots".
		{"node", ""},
		// The blahaj meme look: flat texture, no lighting wash-out.
		{"unlit", true},
	}), spin);
	app += {shark};

	// A single punctual point light near the camera — the PBR pass
	// with USE_PUNCTUAL uploads it as u_Lights[0] (one slot).
	entity lamp(jo("name", "lamp"));
	auto lampTrans = lamp.getTransform();
	lampTrans.setPosition({0.4, 0.6, -1.9});
	lamp += ja(light(jo(
		"type", "point",
		"color", 0xffffffff,
		"intensity", 3.0f)));
	app += {lamp};

	app.start();

	return 0;
}
