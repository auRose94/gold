// The SDL2 window/input/audio backends ship as the libgoldSdl2 plugin
// (window first). This is the parity adapter: gold's windowSystem over
// the SDL2 API, so a machine with only SDL2 still gets windows, input
// and audio instead of degrading to headless. Event mapping matches the
// SDL3 backend's gold event shapes.

#include "game/windowSystem.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <wayland-egl.h>

namespace gold {

	namespace {

		// Matches window::WindowCentered (window.hpp) so the facade's
		// default centering sentinel is honored.
		constexpr int32_t goldCentered = 0x2FFF;

		class sdl2WindowSystem : public windowSystem {
			SDL_Window* _window = nullptr;
			bool _inited = false;
			wl_egl_window* _wlEglWindow = nullptr;

			// Same rationale as the SDL3 backend: bgfx's EGL backend wants
			// a wl_egl_window, created from the wl_surface.
			void initWlEglWindow() {
				if (_wlEglWindow || !_window) return;
				SDL_SysWMinfo info;
				SDL_VERSION(&info.version);
				if (!SDL_GetWindowWMInfo(_window, &info)) return;
				if (info.subsystem != SDL_SYSWM_WAYLAND) return;
				if (!info.info.wl.surface) return;
				int w = 0, h = 0;
				SDL_GetWindowSize(_window, &w, &h);
				_wlEglWindow = wl_egl_window_create(
					info.info.wl.surface, w > 0 ? w : 1, h > 0 ? h : 1);
			}

			bool ensureInit() {
				if (_inited) return true;
				// SDL2 returns 0 on success (SDL3 returns a bool instead).
				if (SDL_Init(SDL_INIT_VIDEO) != 0) {
					fprintf(stderr, "[SDL2] %s\n", SDL_GetError());
					return false;
				}
				_inited = true;
				return true;
			}

			windowEvent fromSDL(const SDL_Event& e) {
				windowEvent out;
				switch (e.type) {
					case SDL_QUIT:
						out.type = windowEvent::eventType::Quit;
						break;
					case SDL_WINDOWEVENT:
						switch (e.window.event) {
							case SDL_WINDOWEVENT_SHOWN:
								out.type = windowEvent::eventType::Shown;
								break;
							case SDL_WINDOWEVENT_MAXIMIZED:
								out.type = windowEvent::eventType::Maximized;
								break;
							case SDL_WINDOWEVENT_RESTORED:
								out.type = windowEvent::eventType::Restored;
								break;
							case SDL_WINDOWEVENT_MINIMIZED:
								out.type = windowEvent::eventType::Minimized;
								break;
							case SDL_WINDOWEVENT_HIDDEN:
								out.type = windowEvent::eventType::Hidden;
								break;
							case SDL_WINDOWEVENT_MOVED:
								out.type = windowEvent::eventType::Moved;
								out.x = e.window.data1;
								out.y = e.window.data2;
								break;
							case SDL_WINDOWEVENT_RESIZED:
							case SDL_WINDOWEVENT_SIZE_CHANGED:
								out.type = windowEvent::eventType::Resized;
								out.width = e.window.data1;
								out.height = e.window.data2;
								break;
							case SDL_WINDOWEVENT_FOCUS_GAINED:
								out.type = windowEvent::eventType::FocusGained;
								break;
							case SDL_WINDOWEVENT_FOCUS_LOST:
								out.type = windowEvent::eventType::FocusLost;
								break;
							default:
								out.type = windowEvent::eventType::NoneEvent;
								break;
						}
						break;
					case SDL_KEYDOWN:
						out.type = windowEvent::eventType::KeyDown;
						out.keyCode = int32_t(e.key.keysym.sym);
						break;
					case SDL_KEYUP:
						out.type = windowEvent::eventType::KeyUp;
						out.keyCode = int32_t(e.key.keysym.sym);
						break;
					case SDL_TEXTINPUT:
						out.type = windowEvent::eventType::TextInput;
						out.text = string(e.text.text);
						break;
					case SDL_MOUSEBUTTONDOWN:
						out.type = windowEvent::eventType::MouseDown;
						out.button = int32_t(e.button.button);
						out.x = int32_t(e.button.x);
						out.y = int32_t(e.button.y);
						break;
					case SDL_MOUSEBUTTONUP:
						out.type = windowEvent::eventType::MouseUp;
						out.button = int32_t(e.button.button);
						out.x = int32_t(e.button.x);
						out.y = int32_t(e.button.y);
						break;
					case SDL_MOUSEMOTION:
						out.type = windowEvent::eventType::MouseMove;
						out.x = int32_t(e.motion.x);
						out.y = int32_t(e.motion.y);
						break;
					case SDL_MOUSEWHEEL:
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

			uint32_t flagsFromConfig(object config) const {
				uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
				if (config.getBool("maximize", false))
					flags |= SDL_WINDOW_MAXIMIZED;
				return flags;
			}

		 public:
			~sdl2WindowSystem() override { destroy(); }

			bool create(object config) override {
				if (!ensureInit()) return false;
				// SDL2 takes x/y in the create call; map the facade's
				// centering sentinel (absent or explicit) onto SDL2's
				// native one.
				const auto x = config.getInt32("x", goldCentered);
				const auto y = config.getInt32("y", goldCentered);
				const int cx = x == goldCentered ? SDL_WINDOWPOS_CENTERED : x;
				const int cy = y == goldCentered ? SDL_WINDOWPOS_CENTERED : y;
				const int32_t w = config.getInt32("width", 1360);
				const int32_t h = config.getInt32("height", 800);
				const string title = config.getString("title", "gold");
				uint32_t flags = flagsFromConfig(config);
				if (config.getBool("borderless", false))
					flags |= SDL_WINDOW_BORDERLESS;
				// The facade's fullscreen: desktop fullscreen when the
				// config asks for it, real mode otherwise.
				if (config.getBool("fullscreen", false))
					flags |= config.getBool("fullscreenDesktop", false)
								 ? SDL_WINDOW_FULLSCREEN_DESKTOP
								 : SDL_WINDOW_FULLSCREEN;
				_window = SDL_CreateWindow(title.c_str(), cx, cy, w, h, flags);
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
					// Narrow quit: SDL_Quit would tear down every SDL
					// subsystem, including ones owned by the input/audio
					// backends sharing this plugin.
					SDL_QuitSubSystem(SDL_INIT_VIDEO);
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
				SDL_SysWMinfo info;
				SDL_VERSION(&info.version);
				if (!SDL_GetWindowWMInfo(_window, &info)) return nw;
				// Prefer Wayland (matches a Wayland session), else X11.
				if (info.subsystem == SDL_SYSWM_WAYLAND) {
					nw.display = info.info.wl.display;
					nw.handle = _wlEglWindow
									? (void*)_wlEglWindow
									: (void*)info.info.wl.surface;
				} else if (info.subsystem == SDL_SYSWM_X11) {
					nw.display = info.info.x11.display;
					nw.handle = (void*)(uintptr_t)info.info.x11.window;
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
				if (!_window) return;
				const uint32_t flags =
					!fullscreen ? 0
								: (desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP
										   : SDL_WINDOW_FULLSCREEN);
				SDL_SetWindowFullscreen(_window, flags);
			}

			void setBorderless(bool borderless) override {
				if (_window)
					SDL_SetWindowBordered(_window,
						borderless ? SDL_FALSE : SDL_TRUE);
			}

			const char* name() const override { return "sdl2"; }
		};

	}  // namespace

}  // namespace gold

// Static registrar: loading libgoldSdl2 registers the backend under its
// concrete name (the generic "sdl" alias belongs to the preferred SDL3
// plugin; the alias chain resolves it when SDL3 is absent).
namespace {
	struct sdl2Registrar {
		sdl2Registrar() {
			auto windowFactory = []() -> gold::windowSystem* {
					return new gold::sdl2WindowSystem();
				};
				gold::registerWindowSystem("sdl2", windowFactory);
				// Also claim the generic "sdl" alias: the candidate chain
				// probes sdl3 first and stops at the first hit, so this
				// only owns the alias on SDL2-only machines.
				gold::registerWindowSystem("sdl", windowFactory);
		}
	} sdl2Reg;
}  // namespace