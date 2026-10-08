#include "game/renderBackend.hpp"
#include "goldtest.hpp"
#include "plugin.hpp"

using namespace gold;

TEST(render_backend_registration_and_invalid_lifecycle) {
	auto bgfx = createRenderBackend(renderBackendType::BGFX);
	EXPECT_TRUE(bgfx != nullptr);
	if (bgfx) {
		EXPECT_EQ(string(bgfx->name()), string("bgfx"));
		EXPECT_FALSE(bgfx->isValid());
		EXPECT_FALSE(bgfx->beginFrame());
		EXPECT_FALSE(bgfx->endFrame());
		bgfx->destroy();
		delete bgfx;
	}

	// The SDL_GPU backend ships in the SDL3 plugin: when it is available,
	// the string registry loads it on demand.
	if (plugin::probe("sdl3")) {
		auto sdl = createRenderBackend("sdlgpu");
		EXPECT_TRUE(sdl != nullptr);
		if (sdl) {
			EXPECT_EQ(string(sdl->name()), string("sdlgpu"));
			EXPECT_FALSE(sdl->isValid());
			EXPECT_FALSE(sdl->beginFrame());
			EXPECT_FALSE(sdl->endFrame());
			sdl->destroy();
			EXPECT_FALSE(sdl->isValid());
			delete sdl;
		}
	}
}

TEST(bgfx_headless_frame_contract) {
	auto backend = createRenderBackend(renderBackendType::BGFX);
	EXPECT_TRUE(backend != nullptr);
	if (!backend) return;

	// RendererType::Noop is zero and does not require a native window.
	auto config = obj({{"rendererType", uint16_t(0)}, {"width", uint32_t(64)},
		{"height", uint32_t(64)}, {"vSync", false}});
	if (backend->initialize(nativeWindow{}, config)) {
		EXPECT_TRUE(backend->isValid());
		EXPECT_TRUE(backend->beginFrame());
		EXPECT_TRUE(backend->endFrame());
		backend->destroy();
		EXPECT_FALSE(backend->isValid());
	} else {
		EXPECT_FALSE(backend->isValid());
	}
	delete backend;
}

int main() {
	return goldtest::runAll();
}
