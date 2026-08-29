#include "game/windowSystem.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <wayland-egl.h>

namespace gold {

	namespace {

		// Matches window::WindowCentered (window.hpp) so the facade's
		// default centering sentinel is honored.
		constexpr int32_t goldCentered = 0x2FFF;

		class sdlWindowSystem : public windowSystem {
			SDL_Window* _window = nullptr;
			bool _inited = false;
			wl_egl_window* _wlEglWindow = nullptr;

			// bgfx's EGL backend expects EGLNativeWindowType, which on
			// Wayland is a wl_egl_window (created from the wl_surface),
			// not the raw surface. SDL3 exposes the surface via window
			// properties but only builds its own wl_egl_window when SDL
			// sets up a GL context itself, so create one here.
			void initWlEglWindow() {
				if (_wlEglWindow || !_window) return;
				auto props = SDL_GetWindowProperties(_window);
				auto wlSurface = SDL_GetPointerProperty(
					props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,
					nullptr);
				if (!wlSurface) return;
				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(_window, &w, &h);
				_wlEglWindow = wl_egl_window_create(
					(wl_surface*)wlSurface, w > 0 ? w : 1, h > 0 ? h : 1);
			}

			bool ensureInit() {
				if (_inited) return true;
				if (!SDL_Init(SDL_INIT_VIDEO)) {
					fprintf(stderr, "[SDL3] %s\n", SDL_GetError());
					return false;
				}
				_inited = true;
				return true;
			}

			windowEvent fromSDL(const SDL_Event& e) {
				windowEvent out;
				switch (e.type) {
					case SDL_EVENT_QUIT:
						out.type = windowEvent::eventType::Quit;
						break;
					case SDL_EVENT_WINDOW_SHOWN:
						out.type = windowEvent::eventType::Shown;
						break;
					case SDL_EVENT_WINDOW_MAXIMIZED:
						out.type = windowEvent::eventType::Maximized;
						break;
					case SDL_EVENT_WINDOW_RESTORED:
						out.type = windowEvent::eventType::Restored;
						break;
					case SDL_EVENT_WINDOW_MINIMIZED:
						out.type = windowEvent::eventType::Minimized;
						break;
					case SDL_EVENT_WINDOW_HIDDEN:
						out.type = windowEvent::eventType::Hidden;
						break;
					case SDL_EVENT_WINDOW_MOVED:
						out.type = windowEvent::eventType::Moved;
						out.x = e.window.data1;
						out.y = e.window.data2;
						break;
					case SDL_EVENT_WINDOW_RESIZED:
						out.type = windowEvent::eventType::Resized;
						out.width = e.window.data1;
						out.height = e.window.data2;
						break;
					case SDL_EVENT_WINDOW_FOCUS_GAINED:
						out.type = windowEvent::eventType::FocusGained;
						break;
					case SDL_EVENT_WINDOW_FOCUS_LOST:
						out.type = windowEvent::eventType::FocusLost;
						break;
					case SDL_EVENT_KEY_DOWN:
						out.type = windowEvent::eventType::KeyDown;
						out.keyCode = int32_t(e.key.key);
						break;
					case SDL_EVENT_KEY_UP:
						out.type = windowEvent::eventType::KeyUp;
						out.keyCode = int32_t(e.key.key);
						break;
					case SDL_EVENT_TEXT_INPUT:
						out.type = windowEvent::eventType::TextInput;
						out.text = string(e.text.text);
						break;
					case SDL_EVENT_MOUSE_BUTTON_DOWN:
						out.type = windowEvent::eventType::MouseDown;
						out.button = int32_t(e.button.button);
						out.x = int32_t(e.button.x);
						out.y = int32_t(e.button.y);
						break;
					case SDL_EVENT_MOUSE_BUTTON_UP:
						out.type = windowEvent::eventType::MouseUp;
						out.button = int32_t(e.button.button);
						out.x = int32_t(e.button.x);
						out.y = int32_t(e.button.y);
						break;
					case SDL_EVENT_MOUSE_MOTION:
						out.type = windowEvent::eventType::MouseMove;
						out.x = int32_t(e.motion.x);
						out.y = int32_t(e.motion.y);
						break;
					case SDL_EVENT_MOUSE_WHEEL:
						out.type = windowEvent::eventType::MouseWheel;
						out.scrollX = int32_t(e.wheel.x);
						out.scrollY = int32_t(e.wheel.y);
						break;
					default:
						out.type = windowEvent::eventType::NoneEvent;
						break;
				}
				return out;
			}

			SDL_WindowFlags flagsFromConfig(object config) {
				SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE |
					SDL_WINDOW_HIGH_PIXEL_DENSITY;
				if (config.getBool("fullscreen", false))
					flags |= SDL_WINDOW_FULLSCREEN;
				if (config.getBool("borderless", false))
					flags |= SDL_WINDOW_BORDERLESS;
				if (config.getBool("maximize", false))
					flags |= SDL_WINDOW_MAXIMIZED;
				return flags;
			}

		 public:
			~sdlWindowSystem() override { destroy(); }

			bool create(object config) override {
				if (!ensureInit()) return false;
				auto x = config.getInt32("x", goldCentered);
				auto y = config.getInt32("y", goldCentered);
				auto w = config.getInt32("width", 1360);
				auto h = config.getInt32("height", 800);
				auto title = config.getString("title", "gold");
				auto flags = flagsFromConfig(config);
				_window = SDL_CreateWindow(title.c_str(), w, h, flags);
				if (_window && !(x == goldCentered && y == goldCentered))
					SDL_SetWindowPosition(_window, x, y);
				if (_window) initWlEglWindow();
				return _window != nullptr;
			}

			void destroy() override {
				if (_wlEglWindow) {
					wl_egl_window_destroy(_wlEglWindow);
					_wlEglWindow = nullptr;
				}
				if (_window) {
					SDL_DestroyWindow(_window);
					_window = nullptr;
				}
				if (_inited) {
					SDL_Quit();
					_inited = false;
				}
			}

			bool poll(object& out) override {
				SDL_Event e;
				if (!SDL_PollEvent(&e)) return false;
				out = eventToObject(fromSDL(e));
				return true;
			}

			nativeWindow native() const override {
				nativeWindow nw;
				if (!_window) return nw;
				nw.window = _window;
				auto props = SDL_GetWindowProperties(_window);
				// Wayland and X11 are the video drivers SDL3 ships on Linux.
				// Prefer Wayland (matches a Wayland session), else X11.
				auto wlEgl = _wlEglWindow
					? (void*)_wlEglWindow
					: SDL_GetPointerProperty(
							props, SDL_PROP_WINDOW_WAYLAND_EGL_WINDOW_POINTER,
							nullptr);
				if (wlEgl) {
					nw.handle = wlEgl;
					nw.display = SDL_GetPointerProperty(
						props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,
						nullptr);
					return nw;
				}
				auto x11Display = SDL_GetPointerProperty(
					props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
				if (x11Display) {
					nw.display = x11Display;
					nw.handle = (void*)SDL_GetNumberProperty(
						props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
				}
				return nw;
			}

			void setTitle(const string& title) override {
				if (_window) SDL_SetWindowTitle(_window, title.c_str());
			}

			void setSize(int32_t width, int32_t height) override {
				if (_window) SDL_SetWindowSize(_window, width, height);
			}

			void setPos(int32_t x, int32_t y) override {
				if (_window) SDL_SetWindowPosition(_window, x, y);
			}

			void setFullscreen(bool fullscreen, bool desktop) override {
				(void)desktop;  // SDL3 fullscreen mode defaults to desktop res
				if (_window) SDL_SetWindowFullscreen(_window, fullscreen);
			}

			void setBorderless(bool borderless) override {
				if (_window)
					SDL_SetWindowBordered(_window, !borderless);
			}

			const char* name() const override { return "sdl"; }
		};

		struct sdlRegistrar {
			sdlRegistrar() {
				registerWindowSystem("sdl", []() -> windowSystem* {
					return new sdlWindowSystem();
				});
				// Second variant: the same SDL3 window backend under an
				// explicit "sdl3" name, so apps/settings can select it
				// without ambiguity.
				registerWindowSystem("sdl3", []() -> windowSystem* {
					return new sdlWindowSystem();
				});
				registerApplicationDataDir(
					[](const string& company,
						 const string& gameName) -> string {
						char* path = SDL_GetPrefPath(
							company.c_str(), gameName.c_str());
						string dir =
							path ? string(path) : string("./");
						SDL_free(path);
						return dir;
					});
			}
		};
		sdlRegistrar sdlReg;

	}  // namespace

}  // namespace gold