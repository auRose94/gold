#include "game/inputSystem.hpp"

#include <libevdev/libevdev.h>
#include <linux/input.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <vector>

namespace gold {

	namespace {

		windowEvent::eventType evType(uint16_t type, uint16_t code,
			int32_t value) {
			switch (type) {
				case EV_KEY:
					return value ? windowEvent::eventType::KeyDown
											 : windowEvent::eventType::KeyUp;
				case EV_REL:
					if (code == REL_WHEEL || code == REL_HWHEEL)
						return windowEvent::eventType::MouseWheel;
					return windowEvent::eventType::MouseMove;
				case EV_ABS:
					return windowEvent::eventType::MouseMove;
				case EV_SYN:
				default:
					return windowEvent::eventType::NoneEvent;
			}
		}

		class evdevInputSystem : public inputSystem {
			std::vector<int> fds;
			std::vector<libevdev*> devs;
			std::vector<windowEvent> pending;
			int32_t absX = 0, absY = 0;

			bool drain(libevdev* dev, int fd) {
				bool any = false;
				input_event ev;
				while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL,
							 &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
					any = true;
					windowEvent out;
					out.type = evType(ev.type, ev.code, ev.value);
					switch (out.type) {
						case windowEvent::eventType::KeyDown:
						case windowEvent::eventType::KeyUp:
							out.keyCode = ev.code;
							pending.push_back(out);
							break;
						case windowEvent::eventType::MouseMove:
							if (ev.type == EV_REL) {
								if (ev.code == REL_X) out.scrollX = ev.value;
								else if (ev.code == REL_Y) out.scrollY = ev.value;
							} else if (ev.type == EV_ABS) {
								if (ev.code == ABS_X) absX = ev.value;
								else if (ev.code == ABS_Y) absY = ev.value;
								out.x = absX;
								out.y = absY;
							}
							if (ev.type == EV_ABS) pending.push_back(out);
							break;
						case windowEvent::eventType::MouseWheel:
							out.scrollY = ev.value;
							pending.push_back(out);
							break;
						default:
							break;
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

			bool poll(windowEvent& out) override {
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