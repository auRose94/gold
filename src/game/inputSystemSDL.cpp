#include "game/inputSystem.hpp"
#include "goldjs.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>

#include <map>

namespace gold {

	namespace {

		// SDL3 input backend: captures the inputs the window event stream
		// does not already carry - gamepads, touch, and sensors - and turns
		// them into gold object events. Keyboard/mouse/window events are
		// left in (re-pushed to) the SDL queue so a window backend can still
		// consume them, keeping the two event sources from fighting.
		class sdlInputSystem : public inputSystem {
			bool _inited = false;
			std::map<SDL_JoystickID, SDL_Gamepad*> gamepads;

			void openConnectedGamepads() {
				int count = 0;
				SDL_JoystickID* ids = SDL_GetGamepads(&count);
				for (int i = 0; i < count; ++i) {
					auto gp = SDL_OpenGamepad(ids[i]);
					if (gp) gamepads[ids[i]] = gp;
				}
				SDL_free(ids);
			}

			void closeGamepad(SDL_JoystickID id) {
				auto it = gamepads.find(id);
				if (it != gamepads.end()) {
					SDL_CloseGamepad(it->second);
					gamepads.erase(it);
				}
			}

			bool toObject(const SDL_Event& e, object& out) {
				switch (e.type) {
					case SDL_EVENT_GAMEPAD_ADDED:
						openConnectedGamepads();
						out = jo("type", "gamepad_added",
							"id", (int64_t)e.gdevice.which);
						return true;
					case SDL_EVENT_GAMEPAD_REMOVED:
						closeGamepad(e.gdevice.which);
						out = jo("type", "gamepad_removed",
							"id", (int64_t)e.gdevice.which);
						return true;
					case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
					case SDL_EVENT_GAMEPAD_BUTTON_UP:
						out = jo("type", "gamepad_button",
							"id", (int64_t)e.gbutton.which,
							"button", (int64_t)e.gbutton.button,
							"down", e.gbutton.down);
						return true;
					case SDL_EVENT_GAMEPAD_AXIS_MOTION:
						out = jo("type", "gamepad_axis",
							"id", (int64_t)e.gaxis.which,
							"axis", (int64_t)e.gaxis.axis,
							"value", (int64_t)e.gaxis.value);
						return true;
					case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
					case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
					case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
						out = jo("type", "gamepad_touchpad",
							"id", (int64_t)e.gtouchpad.which,
							"touchpad", (int64_t)e.gtouchpad.touchpad,
							"finger", (int64_t)e.gtouchpad.finger,
							"x", e.gtouchpad.x,
							"y", e.gtouchpad.y);
						return true;
					case SDL_EVENT_FINGER_DOWN:
					case SDL_EVENT_FINGER_MOTION:
					case SDL_EVENT_FINGER_UP:
					case SDL_EVENT_FINGER_CANCELED: {
						const char* type = "touch_move";
						if (e.type == SDL_EVENT_FINGER_DOWN) type = "touch_down";
						else if (e.type == SDL_EVENT_FINGER_UP) type = "touch_up";
						else if (e.type == SDL_EVENT_FINGER_CANCELED)
							type = "touch_cancel";
						out = jo("type", type,
							"touchId", (int64_t)e.tfinger.touchID,
							"fingerId", (int64_t)e.tfinger.fingerID,
							"x", e.tfinger.x,
							"y", e.tfinger.y,
							"dx", e.tfinger.dx,
							"dy", e.tfinger.dy,
							"pressure", e.tfinger.pressure);
						return true;
					}
					case SDL_EVENT_SENSOR_UPDATE:
						out = jo("type", "sensor",
							"id", (int64_t)e.sensor.which,
							"x", e.sensor.data[0],
							"y", e.sensor.data[1],
							"z", e.sensor.data[2]);
						return true;
					default:
						// Not an input event; put it back so a window
						// backend (or other SDL consumer) can still see it.
						SDL_Event ev = e;
						SDL_PushEvent(&ev);
						return false;
				}
			}

		 public:
			~sdlInputSystem() override { close(); }

			bool open(std::vector<std::string> paths) override {
				(void)paths;  // SDL enumerates devices itself
				if (!_inited) {
					if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_SENSOR)) {
						fprintf(stderr, "[SDL3 input] %s\n", SDL_GetError());
						return false;
					}
					_inited = true;
				}
				openConnectedGamepads();
				return true;
			}

			void close() override {
				for (auto& it : gamepads) SDL_CloseGamepad(it.second);
				gamepads.clear();
				if (_inited) {
					SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_SENSOR);
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

			const char* name() const override { return "sdl"; }
		};

		struct sdlRegistrar {
			sdlRegistrar() {
				registerInputSystem("sdl", []() -> inputSystem* {
					return new sdlInputSystem();
				});
			}
		};
		sdlRegistrar sdlReg;

	}  // namespace

}  // namespace gold