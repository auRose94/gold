#include <iostream>

#include "game/inputSystem.hpp"
#include "game/window.hpp"
#include "game/windowSystem.hpp"
#include "game/light.hpp"
#include "game/sprite.hpp"
#include "game/camera.hpp"
#include "game/transform.hpp"
#include "game/world.hpp"
#include "image.hpp"
#include "goldtest.hpp"

using namespace gold;

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

TEST(window_gold_events) {
	// Headless backend never produces events.
	auto sys = createWindowSystem("headless");
	object ev;
	EXPECT_FALSE(sys->poll(ev));
	EXPECT_EQ(ev.size(), 0);
	delete sys;

	// The window facade dispatches gold-object events through its on*
	// handler slots, updating state with the defaults.
	auto win = window(obj({{"backend", "headless"}}));
	auto err = win.create();
	EXPECT_TRUE(err.isEmpty());
	EXPECT_EQ(string(win.getString("backend")), string("headless"));

	win.handleEvent({obj({
		{"type", "resized"},
		{"width", 1024},
		{"height", 768},
	})});
	EXPECT_EQ(win.getInt32("width"), 1024);
	EXPECT_EQ(win.getInt32("height"), 768);

	win.handleEvent({obj({
		{"type", "moved"},
		{"x", 10},
		{"y", 20},
	})});
	EXPECT_EQ(win.getInt32("x"), 10);
	EXPECT_EQ(win.getInt32("y"), 20);

	win.handleEvent({obj({{"type", "focus_gained"}})});
	EXPECT_TRUE(win.getBool("active"));

	win.handleEvent({obj({{"type", "hidden"}})});
	EXPECT_TRUE(win.getBool("hidden"));

	win.handleEvent({obj({{"type", "key_down"}, {"keyCode", 42}})});
	EXPECT_EQ(win.getInt32("keyCode"), 42);

	win.handleEvent({obj({{"type", "mouse_wheel"}, {"scrollY", -3}})});
	EXPECT_EQ(win.getInt32("scrollY"), -3);

	// onQuit flags the window; unknown event types are ignored.
	win.handleEvent({obj({{"type", "quit"}})});
	EXPECT_TRUE(win.getBool("quit"));
	win.handleEvent({obj({{"type", "completely_made_up"}})});
	EXPECT_TRUE(win.getBool("quit"));

	win.destroy();
}

TEST(window_handler_override) {
	auto win = window(obj({{"backend", "headless"}}));
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
	win.handleEvent({obj({{"type", "resized"}, {"width", 320}})});
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

int main() {
	return goldtest::runAll();
}
