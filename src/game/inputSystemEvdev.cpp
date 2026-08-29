#include "game/inputSystem.hpp"

#include <libevdev/libevdev.h>
#include <linux/input.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <vector>

namespace gold {

	namespace {

		// Maps an evdev event to its gold event type string. REL_X/REL_Y are
		// relative mouse motion, not scroll; wheel deltas are REL_WHEEL/HWHEEL.
		const char* evType(uint16_t type, uint16_t code, int32_t value) {
			switch (type) {
				case EV_KEY:
					return value ? "key_down" : "key_up";
				case EV_REL:
					if (code == REL_WHEEL || code == REL_HWHEEL)
						return "mouse_wheel";
					return "mouse_move";
				case EV_ABS:
					return "mouse_move";
				case EV_SYN:
				default:
					return nullptr;
			}
		}

		class evdevInputSystem : public inputSystem {
			std::vector<int> fds;
			std::vector<libevdev*> devs;
			std::vector<object> pending;
			int32_t absX = 0, absY = 0;

			bool drain(libevdev* dev, int fd) {
				bool any = false;
				input_event ev;
				while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL,
							 &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
					any = true;
					const char* type = evType(ev.type, ev.code, ev.value);
					if (!type) continue;
					object out;
					out.setString("type", type);
					if (type == "key_down" || type == "key_up") {
						out.setInt32("keyCode", ev.code);
						pending.push_back(out);
					} else if (type == "mouse_move") {
						if (ev.type == EV_REL) {
							if (ev.code == REL_X) absX += ev.value;
							else if (ev.code == REL_Y) absY += ev.value;
							out.setInt32("x", absX);
							out.setInt32("y", absY);
							pending.push_back(out);
						} else if (ev.type == EV_ABS) {
							if (ev.code == ABS_X) absX = ev.value;
							else if (ev.code == ABS_Y) absY = ev.value;
							out.setInt32("x", absX);
							out.setInt32("y", absY);
							pending.push_back(out);
						}
					} else if (type == "mouse_wheel") {
						out.setInt32("scrollX", ev.code == REL_HWHEEL ? ev.value : 0);
						out.setInt32("scrollY", ev.code == REL_WHEEL ? ev.value : 0);
						pending.push_back(out);
					}
				}
				(void)fd;
				return any;
			}

		 public:
			~evdevInputSystem() override { close(); }

			bool open(std::vector<std::string> paths) override {
				for (auto& p : paths) {
					int fd = ::open(p.c_str(), O_RDONLY | O_NONBLOCK);
					if (fd < 0) continue;
					libevdev* dev = nullptr;
					if (libevdev_new_from_fd(fd, &dev) != 0) {
						::close(fd);
						continue;
					}
					fds.push_back(fd);
					devs.push_back(dev);
				}
				return !devs.empty();
			}

			void close() override {
				for (auto* dev : devs) {
					libevdev_free(dev);
				}
				for (int fd : fds) {
					::close(fd);
				}
				devs.clear();
				fds.clear();
				pending.clear();
			}

			bool poll(object& out) override {
				for (size_t i = 0; i < devs.size(); ++i)
					drain(devs[i], fds[i]);
				if (pending.empty()) return false;
				out = pending.front();
				pending.erase(pending.begin());
				return true;
			}

			const char* name() const override { return "evdev"; }
		};

		struct evdevRegistrar {
			evdevRegistrar() {
				registerInputSystem("evdev", []() -> inputSystem* {
					return new evdevInputSystem();
				});
			}
		};
		evdevRegistrar evdevReg;

	}  // namespace

}  // namespace gold