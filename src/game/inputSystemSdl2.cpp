// The SDL2 input parity adapter: gamepads (SDL2's game-controller API)
// and touch, with the same gold event shapes as the SDL3 backend. SDL2
// has no sensor events, so sensors are an SDL3-only capability. Keyboard/
// mouse/window events are re-pushed so a window backend still consumes
// them, exactly like the SDL3 backend does.

#include "game/inputSystem.hpp"
#include "goldjs.hpp"

#include <SDL2/SDL.h>

#include <map>

namespace gold {

	namespace {

		class sdl2InputSystem : public inputSystem {
			bool _inited = false;
			std::map<SDL_JoystickID, SDL_GameController*> gamepads;

			void openConnectedGamepads() {
				for (int i = 0; i < SDL_NumJoysticks(); ++i) {
					if (!SDL_IsGameController(i)) continue;
					const auto id = SDL_JoystickGetDeviceInstanceID(i);
					if (id < 0 || gamepads.count(id)) continue;
					if (auto* pad = SDL_GameControllerOpen(i))
						gamepads[(SDL_JoystickID)id] = pad;
				}
			}

			void closeGamepad(SDL_JoystickID id) {
				auto it = gamepads.find(id);
				if (it == gamepads.end()) return;
				SDL_GameControllerClose(it->second);
				gamepads.erase(it);
			}

			bool toObject(const SDL_Event& e, object& out) {
				switch (e.type) {
					case SDL_CONTROLLERDEVICEADDED:
						openConnectedGamepads();
						out = jo("type", "gamepad_added",
							"id", (int64_t)e.cdevice.which);
						return true;
					case SDL_CONTROLLERDEVICEREMOVED:
						closeGamepad(e.cdevice.which);
						out = jo("type", "gamepad_removed",
							"id", (int64_t)e.cdevice.which);
						return true;
					case SDL_CONTROLLERBUTTONDOWN:
					case SDL_CONTROLLERBUTTONUP:
						out = jo("type", "gamepad_button",
							"id", (int64_t)e.cbutton.which,
							"button", (int64_t)e.cbutton.button,
							"down", e.type == SDL_CONTROLLERBUTTONDOWN);
						return true;
					case SDL_CONTROLLERAXISMOTION:
						out = jo("type", "gamepad_axis",
							"id", (int64_t)e.caxis.which,
							"axis", (int64_t)e.caxis.axis,
							"value", (int64_t)e.caxis.value);
						return true;
					case SDL_FINGERDOWN:
					case SDL_FINGERMOTION:
					case SDL_FINGERUP: {
						const char* type = "touch_move";
						if (e.type == SDL_FINGERDOWN) type = "touch_down";
						else if (e.type == SDL_FINGERUP) type = "touch_up";
						out = jo("type", type,
							"touchId", (int64_t)e.tfinger.touchId,
							"fingerId", (int64_t)e.tfinger.fingerId,
							"x", (double)e.tfinger.x,
							"y", (double)e.tfinger.y,
							"dx", (double)e.tfinger.dx,
							"dy", (double)e.tfinger.dy,
							"pressure", (double)e.tfinger.pressure);
						return true;
					}
					default:
						// Not an input event; put it back so a window
						// backend (or other SDL consumer) can still see it.
						SDL_Event ev = e;
						SDL_PushEvent(&ev);
						return false;
				}
			}

		 public:
			~sdl2InputSystem() override { close(); }

			bool open(std::vector<std::string> paths) override {
				(void)paths;  // SDL enumerates devices itself
				if (!_inited) {
					if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
						fprintf(stderr, "[SDL2 input] %s\n", SDL_GetError());
						return false;
					}
					_inited = true;
				}
				openConnectedGamepads();
				return true;
			}

			void close() override {
				for (auto& it : gamepads) SDL_GameControllerClose(it.second);
				gamepads.clear();
				if (_inited) {
					SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
					_inited = false;
				}
			}

			bool poll(object& out) override {
				// Bounded scan: pull events off the queue until one is an
				// input event or we've looked at enough to keep the frame
				// from stalling (non-input events are re-pushed).
				for (int i = 0; i < 16; ++i) {
					SDL_Event e;
					if (!SDL_PollEvent(&e)) return false;
					if (toObject(e, out)) return true;
				}
				return false;
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
			auto inputFactory = []() -> gold::inputSystem* {
					return new gold::sdl2InputSystem();
				};
				gold::registerInputSystem("sdl2", inputFactory);
				gold::registerInputSystem("sdl", inputFactory);
		}
	} sdl2Reg;
}  // namespace