#include "game/renderBackend.hpp"
#include "game/graphics.hpp"
#include "game/renderStateBits.hpp"
#include "shaderSprite.hpp"
#include "goldtest.hpp"
#include "goldjs.hpp"
#include "plugin.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace gold;

namespace {

	// The SDL_GPU backend's headless device path is the offscreen video
	// subsystem; set the env before anything SDL answers to.
	struct envGuard {
		envGuard() { setenv("SDL_VIDEODRIVER", "offscreen", 1); }
	};
}  // namespace

TEST(sdlgpu_backend_initializes_headless) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	EXPECT_TRUE(backend != nullptr);
	if (!backend) return;
	EXPECT_EQ(string(backend->name()), string("sdlgpu"));
	EXPECT_EQ(backend->kind(), rendererKind::Vulkan);
	EXPECT_TRUE(backend->homogeneousDepth());
	EXPECT_FALSE(backend->isValid());
	// A null native window means the offscreen device: SDL's GPU device
	// creation needs no window.
	nativeWindow nw;
	EXPECT_TRUE(backend->initialize(nw, jo("width", 64, "height", 32)));
	EXPECT_TRUE(backend->isValid());
	EXPECT_TRUE(backend->beginFrame());
	EXPECT_TRUE(backend->endFrame());
	backend->destroy();
	EXPECT_FALSE(backend->isValid());
	backend->destroy();  // idempotent
	delete backend;
}

TEST(sdlgpu_uniforms_registry_and_shadow) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	if (!backend || !backend->initialize(nativeWindow(),
			jo("width", 64, "height", 32))) {
		fprintf(stderr, "SKIP: no SDL_GPU device\n");
		EXPECT_TRUE(true);
		return;
	}

	// Distinct names get distinct handles; the same name dedups.
	auto a = backend->createUniform("u_color0", renderUniformType::Vec4);
	auto b = backend->createUniform("u_opacity", renderUniformType::Vec4);
	auto again = backend->createUniform("u_color0", renderUniformType::Vec4);
	EXPECT_TRUE(a.valid());
	EXPECT_TRUE(b.valid());
	EXPECT_EQ(a.idx, again.idx);

	float values[4] = {1, 2, 3, 4};
	backend->setUniform(a, values, 1);
	string name;
	renderUniformType type;
	backend->getUniformInfo(a, name, type);
	EXPECT_EQ(name, string("u_color0"));
	EXPECT_EQ(type, renderUniformType::Vec4);

	backend->destroyUniform(b);
	string bName;
	renderUniformType bType;
	backend->getUniformInfo(b, bName, bType);
	// A destroyed uniform reports no type.
	EXPECT_EQ(bType, renderUniformType::Count);

	backend->destroy();
	delete backend;
}

TEST(sdlgpu_texture_round_trip_readback) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	if (!backend || !backend->initialize(nativeWindow(),
			jo("width", 64, "height", 32))) {
		fprintf(stderr, "SKIP: no SDL_GPU device\n");
		EXPECT_TRUE(true);
		return;
	}

	// A 4x4 RGBA8 texture with known pixels: rows of red, green, blue,
	// white against the raw RGBA byte order the engine uploads.
	uint8_t pixels[16 * 4];
	for (int row = 0; row < 4; ++row)
		for (int col = 0; col < 4; ++col) {
			uint8_t* px = pixels + (row * 4 + col) * 4;
			switch (row) {
			case 0: px[0] = 255; px[1] = 0; px[2] = 0; px[3] = 255; break;
			case 1: px[0] = 0; px[1] = 255; px[2] = 0; px[3] = 255; break;
			case 2: px[0] = 0; px[1] = 0; px[2] = 255; px[3] = 255; break;
			default: px[0] = 255; px[1] = 255; px[2] = 255; px[3] = 255;
			}
		}
	auto tex = backend->createTexture2D(4, 4, false, 1,
		texFormat::RGBA8, 0, pixels, sizeof(pixels));
	EXPECT_TRUE(tex.valid());
	if (!tex.valid()) {
		backend->destroy();
		delete backend;
		return;
	}

	// The upload rides the frame's copy pass; a frame turns before the
	// readback check (the same contract the draw path will use).
	backend->beginFrame();
	backend->endFrame();

	uint8_t readback[16 * 4];
	EXPECT_TRUE(backend->readTexture(tex, readback, 0));
	EXPECT_EQ(0, memcmp(readback, pixels, sizeof(pixels)));

	backend->destroyTexture(tex);
	backend->destroy();
	delete backend;
}

TEST(sdlgpu_embedded_sprite_blob_compiles) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	if (!backend || !backend->initialize(nativeWindow(),
			jo("width", 64, "height", 32))) {
		fprintf(stderr, "SKIP: no SDL_GPU device\n");
		EXPECT_TRUE(true);
		return;
	}

	// The embedded sprite blobs are bgfx's binary-wrapped SPIR-V: the
	// wrapper's table feeds the uniform surface and the SPIR-V rides the
	// descriptor rewrite. Both stages must land. The arrays come straight
	// from the generated header (getSpriteShaderData would need a
	// registered gfxBackend to pick by kind, which a test has none of).
	auto vs = backend->createShader(spirv_vs_sprite.data(),
		uint32_t(spirv_vs_sprite.size()));
	EXPECT_TRUE(vs.valid());
	auto fs = backend->createShader(spirv_fs_sprite.data(),
		uint32_t(spirv_fs_sprite.size()));
	EXPECT_TRUE(fs.valid());
	if (vs.valid() && fs.valid()) {
		auto program = backend->createProgram(vs, fs);
		EXPECT_TRUE(program.valid());

		// The wrapper's table: the fs carries u_color0/u_opacity/s_texColor.
		vector<string> names;
		backend->setShaderUniforms(fs, names);
		EXPECT_TRUE(names.size() >= 3);
		if (names.size() >= 3) {
			EXPECT_EQ(names[0], string("u_color0"));
			EXPECT_EQ(names[1], string("u_opacity"));
			EXPECT_EQ(names[2], string("s_texColor"));
		}
	}

	backend->destroy();
	delete backend;
}

TEST(sdlgpu_compileStage_compiles_source_stages) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	if (!backend || !backend->initialize(nativeWindow(),
			jo("width", 64, "height", 32))) {
		fprintf(stderr, "SKIP: no SDL_GPU device\n");
		EXPECT_TRUE(true);
		return;
	}

	// A minimal .sc with its own io + one uniform: the backend's pp +
	// glslangValidator chain, no bgfx shader-library include involved.
	const auto dir = std::filesystem::temp_directory_path() /
		("gold-sdl-gpu-test-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir);

	const char* varying =
		"vec2 v_texcoord0:TEXCOORD0 = vec2(0.0, 0.0);\n"
		"\n"
		"vec3 a_position:POSITION;\n"
		"vec2 a_texcoord0:TEXCOORD0;\n";
	{
		std::ofstream out(dir / "varying.def.sc");
		out << varying;
	}
	const char* vsc =
		"$input a_position\n"
		"$output v_texcoord0\n"
		"\n"
		"#include <bgfx_shader.sh>\n"
		"\n"
		"void main()\n"
		"{\n"
		"\tgl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));\n"
		"\tv_texcoord0 = a_position.xy;\n"
		"}\n";
	const char* fsc =
		"$input v_texcoord0\n"
		"$output\n"
		"\n"
		"#include <bgfx_shader.sh>\n"
		"\n"
		"uniform vec4 u_color0;\n"
		"\n"
		"void main()\n"
		"{\n"
		"\tgl_FragColor = u_color0;\n"
		"}\n";
	{
		std::ofstream out(dir / "vs_test.sc");
		out << vsc;
		std::ofstream outFs(dir / "fs_test.sc");
		outFs << fsc;
	}

	auto vs = backend->compileStage(jo(
		"type", "vertex",
		"path", (std::string(dir / "vs_test.sc")),
		"defines", "",
		"varying", (std::string(dir / "varying.def.sc")),
		"includeDirs", ja("/usr/include/bgfx")));
	EXPECT_TRUE(vs.valid());
	auto fs = backend->compileStage(jo(
		"type", "fragment",
		"path", (std::string(dir / "fs_test.sc")),
		"defines", "",
		"varying", (std::string(dir / "varying.def.sc")),
		"includeDirs", ja("/usr/include/bgfx")));
	EXPECT_TRUE(fs.valid());
	if (vs.valid() && fs.valid()) {
		auto program = backend->createProgram(vs, fs);
		EXPECT_TRUE(program.valid());
		backend->destroyProgram(program);

		// The fs's uniform surface carries the declared uniform.
		vector<string> names;
		backend->setShaderUniforms(fs, names);
		bool foundColor = false;
		for (auto& n : names)
			if (n == "u_color0") foundColor = true;
		EXPECT_TRUE(foundColor);
	}

	backend->destroyShader(vs);
	backend->destroyShader(fs);
	backend->destroy();
	delete backend;
	std::filesystem::remove_all(dir);
}

TEST(sdlgpu_offscreen_triangle_draw) {
	envGuard guard;
	auto backend = createRenderBackend("sdlgpu");
	if (!backend || !backend->initialize(nativeWindow(),
			jo("width", 64, "height", 32))) {
		fprintf(stderr, "SKIP: no SDL_GPU device\n");
		EXPECT_TRUE(true);
		return;
	}

	// A tiny program: the vs echoes positions (no matrices), the fs
	// writes the u_color0 uniform.
	const auto dir = std::filesystem::temp_directory_path() /
		("gold-sdl-gpu-test-" + std::to_string(getpid()) + "-draw");
	std::filesystem::create_directories(dir);
	{
		std::ofstream out(dir / "varying.def.sc");
		out << "vec4 v_color:COLOR0 = vec4(0.0, 0.0, 0.0, 0.0);\n"
			"\n"
			"vec3 a_position:POSITION;\n";
	}
	{
		std::ofstream out(dir / "vs_tri.sc");
		out << "$input a_position\n"
			"$output v_color\n"
			"\n"
			"#include <bgfx_shader.sh>\n"
			"\n"
			"void main() {\n"
			"\tgl_Position = vec4(a_position, 1.0);\n"
			"\tv_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			"}\n";
		std::ofstream outFs(dir / "fs_tri.sc");
		outFs << "$input v_color\n"
			"$output\n"
			"\n"
			"#include <bgfx_shader.sh>\n"
			"\n"
			"uniform vec4 u_color0;\n"
			"\n"
			"void main() {\n"
			"\tgl_FragColor = u_color0;\n"
			"}\n";
	}

	auto vs = backend->compileStage(jo(
		"type", "vertex",
		"path", std::string(dir / "vs_tri.sc"),
		"defines", "",
		"varying", std::string(dir / "varying.def.sc"),
		"includeDirs", ja("/usr/include/bgfx")));
	EXPECT_TRUE(vs.valid());
	auto fs = backend->compileStage(jo(
		"type", "fragment",
		"path", std::string(dir / "fs_tri.sc"),
		"defines", "",
		"varying", std::string(dir / "varying.def.sc"),
		"includeDirs", ja("/usr/include/bgfx")));
	EXPECT_TRUE(fs.valid());
	if (!vs.valid() || !fs.valid()) {
		backend->destroy();
		delete backend;
		std::filesystem::remove_all(dir);
		return;
	}
	auto program = backend->createProgram(vs, fs);
	EXPECT_TRUE(program.valid());
	auto colorUniform = backend->createUniform(
		"u_color0", renderUniformType::Vec4);
	EXPECT_TRUE(colorUniform.valid());

	// A 64x32 offscreen target.
	auto fb = backend->createFrameBuffer(
		jo("width", 64, "height", 32,
			"format", uint32_t(texFormat::RGBA8)));
	EXPECT_TRUE(fb.valid());
	auto fbTex = backend->getTexture(fb, 0);
	EXPECT_TRUE(fbTex.valid());

	// Vertex layout: position, 3 floats = a 12-byte stride.
	auto layout = vertexLayout()
		.begin()
		.add(vertexLayout::attrib::Position,
			vertexLayout::attribType::Float, 3)
		.end();

	// A triangle in clip space: bottom-left corner down (+y = down in
	// the Vulkan NDC), the wide top edge at y=0.
	float tri[9] = {
		-0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		0.0f, -0.5f, 0.5f,
	};
	vector<uint8_t> vBytes((const uint8_t*)tri,
		(const uint8_t*)tri + sizeof(tri));
	auto vb = backend->createVertexBuffer(vBytes.data(),
		uint32_t(vBytes.size()), layout, 0);
	EXPECT_TRUE(vb.valid());
	uint16_t indices[3] = {0, 2, 1};
	auto ib = backend->createIndexBuffer(indices, sizeof(indices), 0);
	EXPECT_TRUE(ib.valid());

	// Frame 1: create + upload (the copy pass at frame end).
	EXPECT_TRUE(backend->beginFrame());
	// White uniform: the triangle = white; the clear + top = green from
	// the varyings? The fs writes ONLY u_color0. The clear = red.
	float white[4] = {1, 1, 1, 1};
	backend->setUniform(colorUniform, white, 1);
	backend->viewClear(0, ClearColor, 0xFF0000FF, 1.0f, 0);
	backend->viewRect(0, 0, 0, 64, 32);
	backend->setViewFrameBuffer(0, fb);
	if (backend->endFrame()) {
		// Frame 2: bind + draw + readback.
		EXPECT_TRUE(backend->beginFrame());
		backend->setUniform(colorUniform, white, 1);
		backend->viewClear(0, ClearColor, 0xFF0000FF, 1.0f, 0);
		backend->viewRect(0, 0, 0, 64, 32);
		backend->setViewFrameBuffer(0, fb);
		backend->setTextureUniform(0, renderHandle{}, fbTex, 0);
		backend->setState(WriteR | WriteG | WriteB | WriteA
			| uint64_t(DepthAlways) << 5, 0);
		backend->setTransform(nullptr);
		backend->setVertexBuffer(0, vb, 0, 0);
		backend->setIndexBuffer(ib, 0, 0);
		backend->submit(0, program, 0, 0);
		backend->touch(0);

		uint8_t readback[64 * 32 * 4];
		bool ok = true;
		if (backend->endFrame()) {
			ok = backend->readTexture(fbTex, readback, 0);
			if (ok) {
				// The triangle's pixels = white, the top rows = red.
				const uint8_t* px = readback;
				EXPECT_EQ(px[0], 255);
				EXPECT_EQ(px[1], 0);
				EXPECT_EQ(px[2], 0);
				const size_t mid = (16 * 64 + 32) * 4;
				EXPECT_EQ(readback[mid], 255);
				EXPECT_EQ(readback[mid + 1], 255);
				EXPECT_EQ(readback[mid + 2], 255);
			} else
				EXPECT_TRUE(ok);
		}
	}
	backend->destroyBuffer(vb);
	backend->destroyBuffer(ib);
	backend->destroyProgram(program);
	backend->destroyShader(vs);
	backend->destroyShader(fs);
	// The framebuffer's owned textures ride the device destroy.
	backend->destroy();
	delete backend;
	std::filesystem::remove_all(dir);
}

int main() {
	return goldtest::runAll();
}