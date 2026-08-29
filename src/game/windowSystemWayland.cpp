#include "game/windowSystem.hpp"

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gold {

	namespace {

		struct waylandState {
			wl_display* display = nullptr;
			wl_registry* registry = nullptr;
			wl_compositor* compositor = nullptr;
			xdg_wm_base* wmBase = nullptr;
			wl_surface* surface = nullptr;
			xdg_surface* xdgSurface = nullptr;
			xdg_toplevel* toplevel = nullptr;
			wl_seat* seat = nullptr;
			wl_keyboard* keyboard = nullptr;
			wl_pointer* pointer = nullptr;
			int32_t width = 1360;
			int32_t height = 800;
			bool closed = false;
			bool hidden = false;
			bool active = false;
			bool maximized = false;
			std::vector<windowEvent> pending;
		};

		// ---- registry / global interface listeners -------------------

		void registryGlobal(void* data, wl_registry* reg, uint32_t name,
			const char* interface, uint32_t version) {
			auto* st = (waylandState*)data;
			if (strcmp(interface, wl_compositor_interface.name) == 0)
				st->compositor = (wl_compositor*)wl_registry_bind(
					reg, name, &wl_compositor_interface, 1);
			else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
				st->wmBase = (xdg_wm_base*)wl_registry_bind(
					reg, name, &xdg_wm_base_interface, 1);
			else if (strcmp(interface, wl_seat_interface.name) == 0)
				st->seat = (wl_seat*)wl_registry_bind(
					reg, name, &wl_seat_interface, 1);
		}
		void registryGlobalRemove(void*, wl_registry*, uint32_t) {}

		void wmBasePing(void*, xdg_wm_base* base, uint32_t serial) {
			xdg_wm_base_pong(base, serial);
		}

		void xdgSurfaceConfigure(void*, xdg_surface* surf, uint32_t serial) {
			xdg_surface_ack_configure(surf, serial);
		}

		void xdgTopLevelConfigure(void* data, xdg_toplevel*, int32_t w,
			int32_t h, wl_array*) {
			auto* st = (waylandState*)data;
			if (w > 0 && h > 0) {
				st->width = w;
				st->height = h;
				windowEvent ev;
				ev.type = windowEvent::eventType::Resized;
				ev.width = w;
				ev.height = h;
				st->pending.push_back(ev);
			}
		}
		void xdgTopLevelClose(void* data, xdg_toplevel*) {
			auto* st = (waylandState*)data;
			st->closed = true;
			windowEvent ev;
			ev.type = windowEvent::eventType::Quit;
			st->pending.push_back(ev);
		}

		void keyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t) {}
		void keyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*,
			wl_array*) {}
		void keyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*) {}
		void keyboardKey(void* data, wl_keyboard*, uint32_t, uint32_t,
			uint32_t key, uint32_t state) {
			auto* st = (waylandState*)data;
			windowEvent ev;
			ev.type = state ? windowEvent::eventType::KeyDown
											: windowEvent::eventType::KeyUp;
			ev.keyCode = int32_t(key + 8);  // evdev -> linux keycode
			st->pending.push_back(ev);
		}
		void keyboardModifiers(void*, wl_keyboard*, uint32_t, uint32_t,
			uint32_t, uint32_t, uint32_t) {}
		void keyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}

		void pointerEnter(void*, wl_pointer*, uint32_t, wl_surface*,
			wl_fixed_t, wl_fixed_t) {}
		void pointerLeave(void*, wl_pointer*, uint32_t, wl_surface*) {}
		void pointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t x,
			wl_fixed_t y) {
			auto* st = (waylandState*)data;
			windowEvent ev;
			ev.type = windowEvent::eventType::MouseMove;
			ev.x = wl_fixed_to_int(x);
			ev.y = wl_fixed_to_int(y);
			st->pending.push_back(ev);
		}
		void pointerButton(void* data, wl_pointer*, uint32_t, uint32_t,
			uint32_t button, uint32_t state) {
			auto* st = (waylandState*)data;
			windowEvent ev;
			ev.type = state ? windowEvent::eventType::MouseDown
											: windowEvent::eventType::MouseUp;
			ev.button = int32_t(button);
			st->pending.push_back(ev);
		}
		void pointerAxis(void* data, wl_pointer*, uint32_t, uint32_t axis,
			wl_fixed_t value) {
			auto* st = (waylandState*)data;
			windowEvent ev;
			ev.type = windowEvent::eventType::MouseWheel;
			if (axis == 0) ev.scrollX = wl_fixed_to_int(value);
			else ev.scrollY = wl_fixed_to_int(value);
			st->pending.push_back(ev);
		}
								
		const wl_keyboard_listener keyboardListener = {
			keyboardKeymap, keyboardEnter, keyboardLeave, keyboardKey,
			keyboardModifiers, keyboardRepeatInfo,
		};
		const wl_pointer_listener pointerListener = {
			pointerEnter, pointerLeave, pointerMotion, pointerButton,
			pointerAxis,
		};

		const xdg_toplevel_listener toplevelListener = {
			xdgTopLevelConfigure, xdgTopLevelClose,
		};
		const xdg_surface_listener xdgSurfaceListener = {
			xdgSurfaceConfigure,
		};
		const xdg_wm_base_listener wmBaseListener = {wmBasePing};
		const wl_registry_listener registryListener = {
			registryGlobal, registryGlobalRemove,
		};

		class waylandWindowSystem : public windowSystem {
			waylandState st;
			wl_callback* frameCallback = nullptr;

			static void frameDone(void*, wl_callback* cb, uint32_t) {
				wl_callback_destroy(cb);
			}
			static const wl_callback_listener frameListener;

		 public:
			~waylandWindowSystem() override { destroy(); }

			bool create(object config) override {
				st.display = wl_display_connect(nullptr);
				if (!st.display) {
					fprintf(stderr,
						"[Wayland] no compositor (WAYLAND_DISPLAY unset?)\n");
					return false;
				}
				st.registry = wl_display_get_registry(st.display);
				wl_registry_add_listener(
					st.registry, &registryListener, &st);
				wl_display_roundtrip(st.display);

				if (!st.compositor || !st.wmBase) {
					fprintf(stderr,
						"[Wayland] compositor or xdg-shell not available\n");
					destroy();
					return false;
				}

				st.width = config.getInt32("width", 1360);
				st.height = config.getInt32("height", 800);

				st.surface = wl_compositor_create_surface(st.compositor);
				st.xdgSurface = xdg_wm_base_get_xdg_surface(
					st.wmBase, st.surface);
				xdg_surface_add_listener(
					st.xdgSurface, &xdgSurfaceListener, &st);
				st.toplevel = xdg_surface_get_toplevel(st.xdgSurface);
				xdg_toplevel_add_listener(st.toplevel, &toplevelListener, &st);
				xdg_toplevel_set_title(
					st.toplevel, config.getString("title", "gold").c_str());
				xdg_toplevel_set_app_id(st.toplevel, "gold");

				wl_surface_commit(st.surface);
				wl_display_roundtrip(st.display);

				// Input
				if (st.seat) {
					st.keyboard = wl_seat_get_keyboard(st.seat);
					if (st.keyboard)
						wl_keyboard_add_listener(
							st.keyboard, &keyboardListener, &st);
					st.pointer = wl_seat_get_pointer(st.seat);
					if (st.pointer)
						wl_pointer_add_listener(
							st.pointer, &pointerListener, &st);
				}
				return true;
			}

			void destroy() override {
				if (st.keyboard) wl_keyboard_destroy(st.keyboard);
				if (st.pointer) wl_pointer_destroy(st.pointer);
				if (st.toplevel) xdg_toplevel_destroy(st.toplevel);
				if (st.xdgSurface) xdg_surface_destroy(st.xdgSurface);
				if (st.surface) wl_surface_destroy(st.surface);
				if (st.wmBase) xdg_wm_base_destroy(st.wmBase);
				if (st.compositor) wl_compositor_destroy(st.compositor);
				if (st.seat) wl_seat_destroy(st.seat);
				if (st.display) {
					wl_display_flush(st.display);
					wl_display_disconnect(st.display);
				}
				st = waylandState{};
			}

			bool poll(object& out) override {
				wl_display_dispatch_pending(st.display);
				if (st.pending.empty()) return false;
				out = eventToObject(st.pending.front());
				st.pending.erase(st.pending.begin());
				return true;
			}

			nativeWindow native() const override {
				nativeWindow nw;
				nw.handle = (void*)st.surface;
				nw.display = (void*)st.display;
				return nw;
			}

			void setTitle(const string& title) override {
				if (st.toplevel)
					xdg_toplevel_set_title(st.toplevel, title.c_str());
			}
			void setSize(int32_t w, int32_t h) override {
				// xdg-shell has no client-driven resize; the compositor
				// decides. Store the intent and let the configure event
				// reflect the real size.
				st.width = w;
				st.height = h;
			}
			void setPos(int32_t, int32_t) override {
				// No client-side positioning under xdg-shell.
			}
			void setFullscreen(bool fs, bool) override {
				if (st.toplevel) {
					if (fs)
						xdg_toplevel_set_fullscreen(st.toplevel, nullptr);
					else
						xdg_toplevel_unset_fullscreen(st.toplevel);
					wl_surface_commit(st.surface);
				}
			}
			void setBorderless(bool) override {
				// Not expressible with xdg-shell; maximized is the closest.
				if (st.toplevel)
					xdg_toplevel_set_maximized(st.toplevel);
			}

			const char* name() const override { return "wayland"; }
		};

		const wl_callback_listener waylandWindowSystem::frameListener = {
			frameDone,
		};

		struct waylandRegistrar {
			waylandRegistrar() {
				registerWindowSystem("wayland", []() -> windowSystem* {
					return new waylandWindowSystem();
				});
			}
		};
		waylandRegistrar waylandReg;

	}  // namespace

}  // namespace gold