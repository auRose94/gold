#include "game/windowSystem.hpp"

#include <SDL.h>
#include <SDL_syswm.h>

namespace gold {

	namespace {

		class sdlWindowSystem : public windowSystem {
			SDL_Window* _window = nullptr;
			bool _inited = false;

			bool ensureInit() {
				if (_inited) return true;
				SDL_SetMainReady();
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
						out.x = e.button.x;
						out.y = e.button.y;
						break;
					case SDL_MOUSEBUTTONUP:
						out.type = windowEvent::eventType::MouseUp;
						out.button = int32_t(e.button.button);
						out.x = e.button.x;
						out.y = e.button.y;
						break;
					case SDL_MOUSEMOTION:
						out.type = windowEvent::eventType::MouseMove;
						out.x = e.motion.x;
						out.y = e.motion.y;
						break;
					case SDL_MOUSEWHEEL:
						out.type = windowEvent::eventType::MouseWheel;
						out.scrollX = e.wheel.x;
						out.scrollY = e.wheel.y;
						break;
					default:
						out.type = windowEvent::eventType::NoneEvent;
						break;
				}
				return out;
			}

			uint32_t flagsFromConfig(object config) {
				uint32_t flags =
					SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN |
					SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS |
					SDL_WINDOW_ALLOW_HIGHDPI;
				if (config.getBool("fullscreen", false)) {
					flags |= config.getBool("desktop", false)
						? SDL_WINDOW_FULLSCREEN_DESKTOP
						: SDL_WINDOW_FULLSCREEN;
				}
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
				auto x = config.getInt32("x", SDL_WINDOWPOS_CENTERED);
				auto y = config.getInt32("y", SDL_WINDOWPOS_CENTERED);
				auto w = config.getInt32("width", 1360);
				auto h = config.getInt32("height", 800);
				auto title = config.getString("title", "gold");
				_window = SDL_CreateWindow(
					title.c_str(), x, y, w, h, flagsFromConfig(config));
				return _window != nullptr;
			}

			void destroy() override {
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
				if (SDL_PollEvent(&e) == 0) return false;
				out = eventToObject(fromSDL(e));
				return true;
			}

			nativeWindow native() const override {
				nativeWindow nw;
				if (!_window) return nw;
				SDL_SysWMinfo wmi;
				SDL_VERSION(&wmi.version);
				SDL_GetWindowWMInfo(_window, &wmi);
#if defined(BX_PLATFORM_LINUX) || defined(BX_PLATFORM_BSD)
#if defined(ENTRY_CONFIG_USE_WAYLAND)
				nw.handle = (void*)wmi.info.wl.surface;
				nw.display = (void*)wmi.info.wl.display;
#else
				nw.handle = (void*)wmi.info.x11.window;
				nw.display = (void*)wmi.info.x11.display;
#endif
#elif defined(BX_PLATFORM_OSX)
				nw.handle = (void*)wmi.info.cocoa.window;
#elif defined(BX_PLATFORM_WINDOWS)
				nw.handle = (void*)wmi.info.win.window;
#endif
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
				if (_window)
					SDL_SetWindowFullscreen(
						_window,
						fullscreen ? (desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP
																		 : SDL_WINDOW_FULLSCREEN)
											 : 0);
			}

			void setBorderless(bool borderless) override {
				if (_window)
					SDL_SetWindowBordered(_window, SDL_bool(!borderless));
			}

			const char* name() const override { return "sdl"; }
		};

		struct sdlRegistrar {
			sdlRegistrar() {
				registerWindowSystem("sdl", []() -> windowSystem* {
					return new sdlWindowSystem();
				});
				registerApplicationDataDir(
					[](const string& company,
						 const string& gameName) -> string {
						auto dir = SDL_GetPrefPath(
							company.c_str(), gameName.c_str());
						return dir ? string(dir) : string("./");
					});
			}
		};
		sdlRegistrar sdlReg;

	}  // namespace

}  // namespace gold