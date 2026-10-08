// The backend-plugin fixture: a real shared library built by the test
// suite into the executables' directory. plugin::load finds it via the
// search paths (executable dir); loading it runs this registrar, which
// self-registers a fixture window backend — exactly how the produced
// backend plugins (SDL/bgfx/...) register themselves.

#include "game/windowSystem.hpp"

namespace gold {
	struct fixtureWindowSystem : public windowSystem {
		bool create(object) override { return true; }
		void destroy() override {}
		bool poll(object&) override { return false; }
		nativeWindow native() const override { return {}; }
		void setTitle(const string&) override {}
		void setSize(int32_t, int32_t) override {}
		void setPos(int32_t, int32_t) override {}
		void setFullscreen(bool, bool) override {}
		void setBorderless(bool) override {}
		const char* name() const override { return "fixtureWindow"; }
	};
}  // namespace gold

namespace {
	struct fixtureRegistrar {
		fixtureRegistrar() {
			gold::registerWindowSystem("fixtureWindow",
				[]() -> gold::windowSystem* {
					return new gold::fixtureWindowSystem();
				});
		}
	} fixtureReg;
}  // namespace