#include "game/windowSystem.hpp"

namespace gold {

	namespace {
		// A minimal backend with no real window. Useful for headless
		// rendering, tests, and CI. poll() always reports quit=false and
		// never blocks; native() returns null handles (renderers that need a
		// real surface will fall back to an offscreen/headless path).
		class headlessWindowSystem : public windowSystem {
		 public:
			~headlessWindowSystem() override = default;

			bool create(object) override { return true; }
			void destroy() override {}

			bool poll(object& out) override {
				out = object();
				return false;
			}

			nativeWindow native() const override { return nativeWindow{}; }

			void setTitle(const string&) override {}
			void setSize(int32_t, int32_t) override {}
			void setPos(int32_t, int32_t) override {}
			void setFullscreen(bool, bool) override {}
			void setBorderless(bool) override {}

			const char* name() const override { return "headless"; }
		};

		struct headlessRegistrar {
			headlessRegistrar() {
				registerWindowSystem("headless", []() -> windowSystem* {
					return new headlessWindowSystem();
				});
			}
		};
		headlessRegistrar headlessReg;
	}  // namespace

}  // namespace gold