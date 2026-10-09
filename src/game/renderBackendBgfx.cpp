#include "game/renderBackend.hpp"

#include <bgfx/bgfx.h>
#if __has_include(<bgfx/platform.h>)
#include <bgfx/platform.h>
#endif

#include <bx/allocator.h>
#include <bx/readerwriter.h>
#include <bimg/bimg.h>
#include <bimg/encode.h>

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game/graphics.hpp"
#include "game/renderStateBits.hpp"

namespace gold {

	using namespace std;

	namespace {
		// The gold flag-word translations. The parsers (graphics.cpp) build
		// the gold-native bits of renderStateBits.hpp; these functions turn
		// them into bgfx's define values. Written explicitly so any layout
		// drift is a visible bug here.

		// --- draw state --------------------------------------------------
		uint64_t goldFactorToBGFX(uint64_t v) {
			// gold's factor ids sit at 0 (zero); bgfx's factor field uses
			// 1 (zero) with 0 meaning "leave as-is" — hence +1.
			return (v & 0xF) + 1;
		}
		uint64_t goldCompareToBGFX(uint64_t v) {
			// gold: 0=always, 1=less, ..., 7=never. bgfx's depth-test field
			// uses 0 = no test and has no always: always maps to no test,
			// others are 1..7 as written.
			switch (v & 0x7) {
			case uint64_t(DepthAlways): return 0;  // test-off passes always
			default: return (v & 0x7);  // less..never align 1..7
			}
		}
		uint64_t goldBlendEquationToBGFX(uint64_t v) {
			// add=0, sub=1, revsub=2, min=3, max=4 — the same values.
			return v & 0x7;
		}

		uint64_t fromGoldState(uint64_t gold) {
			uint64_t out = 0;
			if (gold & WriteR) out |= BGFX_STATE_WRITE_R;
			if (gold & WriteG) out |= BGFX_STATE_WRITE_G;
			if (gold & WriteB) out |= BGFX_STATE_WRITE_B;
			if (gold & WriteA) out |= BGFX_STATE_WRITE_A;
			if (gold & WriteZ) out |= BGFX_STATE_WRITE_Z;
			const auto compare = goldCompareToBGFX(
				gold >> DepthCompareShift);
			out |= compare << BGFX_STATE_DEPTH_TEST_SHIFT;
			const auto cull = (gold >> CullShift) & 0x3;
			if (cull == uint64_t(CullCW)) out |= BGFX_STATE_CULL_CW;
			else if (cull == uint64_t(CullCCW)) out |= BGFX_STATE_CULL_CCW;
			if (gold & BlendEnabled) {
				// bgfx packs four 4-bit factor fields at shift 12 (rgb-src,
				// rgb-dst, a-src, a-dst) and the equation at shift 28. gold's
				// factor ids are 0-based; bgfx's 1-based.
				const uint64_t rgbSrc =
					goldFactorToBGFX(gold >> BlendRGBSrcShift);
				const uint64_t rgbDst =
					goldFactorToBGFX(gold >> BlendRGBDstShift);
				const uint64_t aSrc =
					goldFactorToBGFX(gold >> BlendASrcShift);
				const uint64_t aDst =
					goldFactorToBGFX(gold >> BlendADstShift);
				out |= ((rgbSrc << 0) | (rgbDst << 4) | (aSrc << 8) |
						(aDst << 12))
					   << BGFX_STATE_BLEND_SHIFT;
				out |= goldBlendEquationToBGFX(gold >> BlendEquationShift)
					   << BGFX_STATE_BLEND_EQUATION_SHIFT;
			}
			if (gold & BlendIndependent) out |= BGFX_STATE_BLEND_INDEPENDENT;
			if (gold & BlendAlphaToCoverage)
				out |= BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;
			const auto ref = (gold >> AlphaRefShift) & 0xFF;
			out |= BGFX_STATE_ALPHA_REF(ref);
			const auto prim = (gold >> PrimitiveShift) & 0x7;
			switch (prim) {
			case uint64_t(PrimitiveTriStrip): out |= BGFX_STATE_PT_TRISTRIP; break;
			case uint64_t(PrimitiveLines): out |= BGFX_STATE_PT_LINES; break;
			case uint64_t(PrimitiveLineStrip): out |= BGFX_STATE_PT_LINESTRIP; break;
			case uint64_t(PrimitivePoints): out |= BGFX_STATE_PT_POINTS; break;
			default: break;  // triangle list = no bits
			}
			if (gold & StateMSAA) out |= BGFX_STATE_MSAA;
			if (gold & StateLineAA) out |= BGFX_STATE_LINEAA;
			if (gold & StateConservativeRaster)
				out |= BGFX_STATE_CONSERVATIVE_RASTER;
			return out;
		}

		// --- stencil -------------------------------------------------------
		uint32_t goldStencilOpToBGFX(uint32_t v) {
			// gold: 0=keep 1=zero 2=replace 3=incr 4=incr-sat 5=decr
			// 6=decr-sat 7=invert. bgfx: 0=zero 1=keep — swap the pair,
			// the rest match.
			switch (v & 0x7) {
			case uint32_t(OpKeep): return 1;
			case uint32_t(OpZero): return 0;
			default: return v & 0x7;
			}
		}
		uint32_t goldCompareFieldToBGFXStencil(uint32_t v) {
			// gold: 0=always..7=never -> bgfx stencil test: 1..7 + always=8.
			switch (v & 0x7) {
			case uint32_t(DepthAlways): return 8;
			default: return v & 0x7;
			}
		}
		uint32_t fromGoldStencil(uint32_t gold) {
			if (gold == 0) return UINT32_C(0x0000ff00);  // BGFX_STENCIL_NONE
			uint32_t out = BGFX_STENCIL_FUNC_RMASK(0xFF);
			out |= goldCompareFieldToBGFXStencil(gold >> StencilTestShift)
				   << BGFX_STENCIL_TEST_SHIFT;
			out |= goldStencilOpToBGFX(gold >> StencilFailShift)
				   << BGFX_STENCIL_OP_FAIL_S_SHIFT;
			out |= goldStencilOpToBGFX(gold >> StencilZFailShift)
				   << BGFX_STENCIL_OP_FAIL_Z_SHIFT;
			out |= goldStencilOpToBGFX(gold >> StencilZPassShift)
				   << BGFX_STENCIL_OP_PASS_Z_SHIFT;
			return out;
		}

		// --- samplers ------------------------------------------------------
		uint32_t goldWrapToBGFX(uint32_t v, int shift) {
			// gold: bits (mirror|clamp|border of 1|2|4) per axis; bgfx:
			// a 2-bit field at `shift` with mirror=1 clamp=2 border=3.
			uint64_t mode = v & 0x7;
			if (mode & 1) mode = 1;
			else if (mode & 2) mode = 2;
			else if (mode & 4) mode = 3;
			else mode = 0;
			return uint32_t(mode) << shift;
		}
		uint32_t fromGoldSampler(uint32_t gold) {
			uint32_t out = 0;
			out |= goldWrapToBGFX(gold & 0x7, BGFX_SAMPLER_U_SHIFT);
			out |= goldWrapToBGFX((gold >> 3) & 0x7, BGFX_SAMPLER_V_SHIFT);
			out |= goldWrapToBGFX((gold >> 6) & 0x7, BGFX_SAMPLER_W_SHIFT);
			if (gold & MinPoint) out |= BGFX_SAMPLER_MIN_POINT;
			if (gold & MinAnisotropic) out |= BGFX_SAMPLER_MIN_ANISOTROPIC;
			if (gold & MagPoint) out |= BGFX_SAMPLER_MAG_POINT;
			if (gold & MagAnisotropic) out |= BGFX_SAMPLER_MAG_ANISOTROPIC;
			if (gold & MipPoint) out |= BGFX_SAMPLER_MIP_POINT;
			if (gold & CompareEnabled) {
				uint32_t mode = (gold >> CompareModeShift) & 0x7;
				static const uint32_t compareMap[8] = {
					0x00080000,  // always
					0x00010000,  // less
					0x00020000,  // lequal
					0x00030000,  // equal
					0x00040000,  // gequal
					0x00050000,  // greater
					0x00060000,  // notequal
					0x00070000,  // never
				};
				out |= compareMap[mode];
			}
			return out;
		}

		// Map a gold-native texFormat to a bgfx TextureFormat.
		bgfx::TextureFormat::Enum toBGFX(texFormat f) {
			return (bgfx::TextureFormat::Enum)uint8_t(f);
		}
		// The uniform and vertex-attribute type maps are explicit: gold's
		// value layouts are its own, so a backend cannot drift by casting.
		bgfx::UniformType::Enum toBGFXUniform(renderUniformType t) {
			switch (t) {
			case renderUniformType::Sampler:
				return bgfx::UniformType::Sampler;
			case renderUniformType::Vec4:
				return bgfx::UniformType::Vec4;
			case renderUniformType::Mat3:
				return bgfx::UniformType::Mat3;
			case renderUniformType::Mat4:
				return bgfx::UniformType::Mat4;
			default:
				return bgfx::UniformType::Count;
			}
		}
		renderUniformType fromBGFXUniform(bgfx::UniformType::Enum t) {
			switch (t) {
			case bgfx::UniformType::Sampler:
				return renderUniformType::Sampler;
			case bgfx::UniformType::Vec4:
				return renderUniformType::Vec4;
			case bgfx::UniformType::Mat3:
				return renderUniformType::Mat3;
			case bgfx::UniformType::Mat4:
				return renderUniformType::Mat4;
			default:
				return renderUniformType::Count;
			}
		}
		bgfx::AttribType::Enum toBGFXAttribType(vertexAttribType t) {
			switch (t) {
			case vertexAttribType::Int8:
				return bgfx::AttribType::Int8;
			case vertexAttribType::Uint8:
				return bgfx::AttribType::Uint8;
			case vertexAttribType::Uint10:
				return bgfx::AttribType::Uint10;
			case vertexAttribType::Int16:
				return bgfx::AttribType::Int16;
			case vertexAttribType::Uint16:
				return bgfx::AttribType::Uint16;
			case vertexAttribType::Half:
				return bgfx::AttribType::Half;
			case vertexAttribType::Float:
				return bgfx::AttribType::Float;
			case vertexAttribType::Int32:
				return bgfx::AttribType::Int32;
			case vertexAttribType::Uint32:
				return bgfx::AttribType::Uint32;
			default:
				return bgfx::AttribType::Count;
			}
		}

		// Compile a .sc shader with the bgfx shaderc tool (an external
		// program built by bgfx.cmake, or the system bgfx-shaderc). The
		// compiled .bin is read back from stdout. GOLD_SHADER_COMPILER is
		// the tool path; GOLD_BGFX_SHADER_INCLUDE the shader header root.
		// Both bake into THIS plugin: no other backend shares the
		// toolchain.
		binary compileStageBin(
			char type, const char* filePath, const char* defines,
			const char* varyingPath, const char* profile,
			const vector<string>& includeDirs) {
			const char* typeName = type == 'v' ? "vertex"
				: type == 'f' ? "fragment" : "compute";
			string prof = (profile && *profile) ? string(profile)
				: (type == 'c' ? string("430") : string("330"));
			vector<string> args = {
				GOLD_SHADER_COMPILER,
				"-f", filePath,
				"--type", typeName,
				"--platform", "linux",
				"--profile", prof,
				// bgfx's own shader headers (bgfx_shader.sh and friends),
				// then caller-supplied dirs — the generated source lives in
				// a temp dir, so the shader library dir must come from the
				// caller, not from the input's directory.
				"-i", GOLD_BGFX_SHADER_INCLUDE,
				"--stdout",
			};
			for (const auto& dir : includeDirs)
				if (!dir.empty()) {
					args.push_back("-i");
					args.push_back(dir);
				}
			if (defines && *defines) {
				args.push_back("--define");
				args.push_back(defines);
			}
			if (varyingPath && *varyingPath
				&& filesystem::exists(filesystem::path(varyingPath))) {
				// Expand the varying's conditionals (some shaderc vintages
				// reject directives there); the expanded file satisfies all
				// of them and keeps vertex layouts defined-conditional.
				const auto expanded = expandVaryingDefinition(
					varyingPath, defines ? defines : "");
				if (!expanded.empty()) {
					args.push_back("--varyingdef");
					args.push_back(expanded);
				}
			}

			vector<char*> argv;
			argv.reserve(args.size() + 1);
			for (auto& a : args) argv.push_back(a.data());
			argv.push_back(nullptr);

			int fds[2];
			if (pipe(fds) != 0) return binary();
			pid_t pid = fork();
			if (pid == 0) {
				dup2(fds[1], STDOUT_FILENO);
				close(fds[0]);
				close(fds[1]);
				execvp(GOLD_SHADER_COMPILER, argv.data());
				_exit(127);
			}
			close(fds[1]);
			vector<uint8_t> out;
			char buf[65536];
			ssize_t n;
			while ((n = read(fds[0], buf, sizeof(buf))) > 0)
				out.insert(out.end(), buf, buf + n);
			close(fds[0]);
			int status = 0;
			waitpid(pid, &status, 0);
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || out.empty()) {
				// Surface the tool's own message so compile failures explain
				// themselves instead of silently skipping a program.
				cerr << "Shader compile failed (" << filePath << "):\n";
				cerr.write(reinterpret_cast<const char*>(out.data()),
					(std::streamsize)out.size());
				return binary();
			}
			return binary(out.begin(), out.end());
		}

		class bgfxRenderBackend : public renderBackend {
			bool _valid = false;
			bgfx::PlatformData _pd;

			static renderHandle toHandle(bgfx::TextureHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::ProgramHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::UniformHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::VertexBufferHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::IndexBufferHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::FrameBufferHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::OcclusionQueryHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::IndirectBufferHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::ShaderHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::DynamicVertexBufferHandle h) {
				return renderHandle{h.idx};
			}
			static renderHandle toHandle(bgfx::DynamicIndexBufferHandle h) {
				return renderHandle{h.idx};
			}

			static bgfx::TextureHandle tex(renderHandle h) {
				return bgfx::TextureHandle{h.idx};
			}
			static bgfx::ProgramHandle prog(renderHandle h) {
				return bgfx::ProgramHandle{h.idx};
			}
			static bgfx::UniformHandle uni(renderHandle h) {
				return bgfx::UniformHandle{h.idx};
			}
			static bgfx::VertexBufferHandle vb(renderHandle h) {
				return bgfx::VertexBufferHandle{h.idx};
			}
			static bgfx::IndexBufferHandle ib(renderHandle h) {
				return bgfx::IndexBufferHandle{h.idx};
			}
			static bgfx::FrameBufferHandle fb(renderHandle h) {
				return bgfx::FrameBufferHandle{h.idx};
			}
			static bgfx::OcclusionQueryHandle oq(renderHandle h) {
				return bgfx::OcclusionQueryHandle{h.idx};
			}
			static bgfx::IndirectBufferHandle ibuf(renderHandle h) {
				return bgfx::IndirectBufferHandle{h.idx};
			}
			static bgfx::ShaderHandle sh(renderHandle h) {
				return bgfx::ShaderHandle{h.idx};
			}

			renderBackendType _type = renderBackendType::BGFX;
			uint16_t _rendererType = uint16_t(bgfx::RendererType::Noop);

			static const bgfx::Memory* mem(const void* data, uint32_t size) {
				return bgfx::makeRef(data, size);
			}

		 public:
			~bgfxRenderBackend() override { destroy(); }

			renderBackendType type() const override { return _type; }
			const char* name() const override { return "bgfx"; }
			rendererKind kind() const override {
				// Recorded at initialize; before it runs, the noop kind.
				switch ((bgfx::RendererType::Enum)_rendererType) {
				case bgfx::RendererType::Direct3D11:
					return rendererKind::D3D11;
				case bgfx::RendererType::Direct3D12:
					return rendererKind::D3D12;
				case bgfx::RendererType::Metal:
					return rendererKind::Metal;
				case bgfx::RendererType::OpenGLES:
					return rendererKind::GLES;
				case bgfx::RendererType::OpenGL:
					return rendererKind::GL;
				case bgfx::RendererType::Vulkan:
					return rendererKind::Vulkan;
				default:
					return rendererKind::Noop;
				}
			}

			// The application callback: traces to the console, and screen
			// shots become PNG files via the bundled image codecs (bgfx's
			// default handler receives shots and does nothing with them).
			class goldCallbacks : public bgfx::CallbackI {
				void fatal(const char*, uint16_t, bgfx::Fatal::Enum code,
					const char* message) override {
					fprintf(
						stderr, "[bgfx fatal %d] %s\n", (int)code, message);
					abort();
				}

				void traceVargs(const char*, uint16_t, const char* format,
					va_list arguments) override {
					vfprintf(stderr, format, arguments);
				}

				void profilerBegin(const char*, uint32_t, const char*,
					uint16_t) override {}
				void profilerBeginLiteral(const char*, uint32_t,
					const char*, uint16_t) override {}
				void profilerEnd() override {}
				bool cacheRead(uint64_t, void*, uint32_t) override {
					return false;
				}
				void cacheWrite(uint64_t, const void*, uint32_t) override {}
				uint32_t cacheReadSize(uint64_t) override { return 0; }
				void captureBegin(uint32_t, uint32_t, uint32_t,
					bgfx::TextureFormat::Enum, bool) override {}
				void captureEnd() override {}
				void captureFrame(const void*, uint32_t) override {}

				void screenShot(const char* filePath, uint32_t width,
					uint32_t height, uint32_t pitch,
					bgfx::TextureFormat::Enum format, const void* data,
					uint32_t, bool yflip) override {
					bx::DefaultAllocator allocator;
					auto block = bx::MemoryBlock(&allocator);
					auto writer = bx::MemoryWriter(&block);
					auto error = bx::Error();
					const auto bytes = bimg::imageWritePng(
						&writer, uint16_t(width), uint16_t(height),
						(uint32_t)pitch, const_cast<void*>(data),
						bimg::TextureFormat::Enum(format), yflip, &error);
					if (!error.isOk()) {
						fprintf(
							stderr, "[bgfx shot] PNG write failed\n");
						return;
					}
					std::ofstream out(filePath, std::ofstream::binary);
					out.write(
						(const char*)block.more(),
						(std::streamsize)bytes);
					fprintf(stderr, "[bgfx shot] saved %s (%ux%u)\n",
						filePath, width, height);
				}
			};
			goldCallbacks _callbacks;

			bool initialize(nativeWindow nw, object config) override {
				if (nw.handle || nw.display) {
					_pd = bgfx::PlatformData();
					_pd.nwh = nw.handle;
					_pd.ndt = nw.display;
					_pd.context = nw.context;
					bgfx::setPlatformData(_pd);
				}
				bgfx::Init init = bgfx::Init();
				init.platformData = _pd;
				init.callback = &_callbacks;
				{
					// "rendererType" is gold data: a numeric entry casts
					// (a bgfx RendererType number, Noop=0 keeps the
					// headless contract), a string parses.
					auto requested = config.getVar("rendererType");
					auto type = bgfx::RendererType::OpenGL;
					if (requested.isNumber()) {
						type = bgfx::RendererType::Enum(
							requested.getUInt16());
					} else if (requested.isString()) {
						auto str = requested.getString();
						std::transform(str.begin(), str.end(),
							str.begin(),
							[](unsigned char c) {
								return (char)std::tolower(c);
							});
						if (str == "noop") type = bgfx::RendererType::Noop;
						else if (str == "direct3d11")
							type = bgfx::RendererType::Direct3D11;
						else if (str == "direct3d12")
							type = bgfx::RendererType::Direct3D12;
						else if (str == "gnm")
							type = bgfx::RendererType::Gnm;
						else if (str == "metal")
							type = bgfx::RendererType::Metal;
						else if (str == "nvn")
							type = bgfx::RendererType::Nvn;
						else if (str == "opengles")
							type = bgfx::RendererType::OpenGLES;
						else if (str == "opengl")
							type = bgfx::RendererType::OpenGL;
						else if (str == "vulkan")
							type = bgfx::RendererType::Vulkan;
					}
					init.type = type;
				}
				init.vendorId = BGFX_PCI_ID_NONE;
				init.resolution.width = config.getUInt32("width", 1360);
				init.resolution.height = config.getUInt32("height", 800);
				init.resolution.reset =
					(config.getBool("vSync", true) ? BGFX_RESET_VSYNC : 0) |
					(config.getBool("maxAnisotropy", false)
						 ? BGFX_RESET_MAXANISOTROPY
						 : 0);
				_valid = bgfx::init(init);
				if (_valid) {
					_rendererType = uint16_t(bgfx::getRendererType());
					bgfx::setDebug(
						(config.getBool("debug", false) ? BGFX_DEBUG_TEXT : 0) |
						(config.getBool("stats", false) ? BGFX_DEBUG_STATS : 0));
				}
				return _valid;
			}

			void destroy() override {
				if (_valid) {
					bgfx::shutdown();
					_valid = false;
				}
			}

			bool isValid() const override { return _valid; }

			bool beginFrame() override {
				// beginFrame only opens the submission window. Advancing bgfx
				// here would present the previous frame before the caller has
				// finished submitting the current one.
				return _valid;
			}
			bool endFrame() override {
				if (!_valid) return false;
				bgfx::frame(false);
				return true;
			}

			void viewRect(uint8_t view, uint16_t x, uint16_t y, uint16_t w,
				uint16_t h) override {
				bgfx::setViewRect(view, x, y, w, h);
			}
			void viewClear(uint8_t view, uint16_t flags, uint32_t rgba,
				float depth, uint8_t stencil) override {
				bgfx::setViewClear(view, flags, rgba, depth, stencil);
			}
			void viewTransform(uint8_t view, const void* viewMtx,
				const void* projMtx) override {
				bgfx::setViewTransform(view, viewMtx, projMtx);
			}

			renderHandle createShader(const void* data,
				uint32_t size) override {
				// An owning copy: the caller's buffer may be a short-lived
				// compile result, and bgfx consumes shader memory on its
				// render thread after this call returns.
				return toHandle(
					bgfx::createShader(bgfx::copy(data, size)));
			}
			renderHandle createProgram(renderHandle vs,
				renderHandle fs) override {
				return toHandle(bgfx::createProgram(sh(vs), sh(fs)));
			}
			// Backends own their compiler: this shells the bgfx-shaderc
			// toolchain and returns the created stage.
			renderHandle compileStage(object request) override {
				auto typeName = request["type"].getString();
				char type = typeName == "vertex" ? 'v'
					: typeName == "fragment" ? 'f' : 'c';
				auto path = request["path"].getString();
				auto defines = request["defines"].getString();
				auto varying = request["varying"].getString();
				auto includeDirs = vector<string>();
				for (const auto& dir :
					request.getList("includeDirs", list()))
					includeDirs.push_back(dir.getString());
				auto bin = compileStageBin(type, path.c_str(),
					defines.c_str(), varying.c_str(), nullptr, includeDirs);
				if (bin.empty()) return renderHandle{};
				return createShader(bin.data(), uint32_t(bin.size()));
			}
			renderHandle createUniform(const char* name, renderUniformType t,
				uint16_t num) override {
				return toHandle(
					bgfx::createUniform(name, toBGFXUniform(t), num));
			}
			void setUniform(renderHandle h, const void* value,
				uint16_t num) override {
				bgfx::setUniform(uni(h), value, num);
			}
			void getUniformInfo(renderHandle h, string& name,
				renderUniformType& t) const override {
				bgfx::UniformInfo info;
				bgfx::getUniformInfo(uni(h), info);
				name = info.name;
				t = fromBGFXUniform(info.type);
			}
			void setShaderUniforms(renderHandle h,
				vector<string>& out) override {
				// Query count first, then fill handles and map to names.
				uint16_t count =
					bgfx::getShaderUniforms(sh(h), nullptr, 0);
				std::vector<bgfx::UniformHandle> handles(count);
				bgfx::getShaderUniforms(sh(h), handles.data(), count);
				for (auto& uh : handles) {
					bgfx::UniformInfo info;
					bgfx::getUniformInfo(uh, info);
					out.push_back(info.name);
				}
			}

			renderHandle createTexture2D(uint16_t w, uint16_t h,
				bool hasMips, uint16_t numLayers, texFormat f, uint64_t flags,
				const void* data, uint32_t size) override {
				return toHandle(bgfx::createTexture2D(w, h, hasMips,
					numLayers, toBGFX(f), fromGoldSampler(uint32_t(flags)),
					mem(data, size)));
			}
			renderHandle createTextureCube(uint16_t size, bool hasMips,
				uint16_t numLayers, texFormat f, uint64_t flags,
				const void* data, uint32_t size_) override {
				return toHandle(bgfx::createTextureCube(size, hasMips,
					numLayers, toBGFX(f),
					fromGoldSampler(uint32_t(flags)), mem(data, size_)));
			}
			renderHandle createTexture3D(uint16_t w, uint16_t h, uint16_t d,
				bool hasMips, texFormat f, uint64_t flags, const void* data,
				uint32_t size) override {
				// Volume textures; the whole volume ships with the data.
				return toHandle(bgfx::createTexture3D(
					w, h, d, hasMips, toBGFX(f),
					fromGoldSampler(uint32_t(flags)), mem(data, size)));
			}
			void updateTexture2D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				bgfx::updateTexture2D(
					tex(h), 0, mip, 0, 0, 0, 0, bgfx::makeRef(data, size));
			}
			void updateTexture3D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				bgfx::updateTexture3D(
					tex(h), 0, mip, 0, 0, 0, 0, 0, bgfx::makeRef(data, size));
			}
			void updateTexture(renderHandle h, uint8_t side, uint8_t mip,
				const void* data, uint32_t size) override {
				bgfx::updateTextureCube(tex(h), side, mip, 0, 0, 0, 0, 0,
					bgfx::makeRef(data, size));
			}
			void destroyTexture(renderHandle h) override {
				bgfx::destroy(tex(h));
			}
			bool readTexture(renderHandle h, void* data,
				uint8_t mip) override {
				bgfx::TextureRegion region;
				region.handle = tex(h);
				region.mip = mip;
				bgfx::read(region, data);
				return true;
			}
			void* directAccessPtr(renderHandle h) override {
				return bgfx::getDirectAccessPtr(tex(h));
			}

			/** Materialize a gold layout descriptor — a vertexLayout object
			 *  whose "descriptor" list carries {attrib, count, type,
			 *  normalized, asInt} entries — into a bgfx layout. bgfx's add()
			 *  takes (attrib, count, type, asInt, normalized); the flags
			 *  land in the right slots (the previous facade code passed
			 *  the pair swapped, which was invisible while every caller
			 *  sent both false). */
			static bgfx::VertexLayout materializeLayout(object layoutObj) {
				bgfx::VertexLayout layout;
				layout.begin();
				if (layoutObj) {
					auto entries = layoutObj.getList("descriptor");
					for (auto& entry : entries) {
						auto e = entry.getObject();
						layout.add(
							(bgfx::Attrib::Enum)e.getUInt8("attrib"),
							e.getUInt8("count"),
							toBGFXAttribType(
								(vertexAttribType)e.getUInt8("type")),
							e.getBool("asInt"), e.getBool("normalized"));
					}
				}
				layout.end();
				return layout;
			}

			// The transient chunks handed out under gold handles, so their
			// update and set can reach the backing structures again.
			std::map<renderHandle, std::unique_ptr<bgfx::TransientVertexBuffer>>
				transientVBs;
			std::map<renderHandle, std::unique_ptr<bgfx::TransientIndexBuffer>>
				transientIBs;

			renderHandle createVertexBuffer(const void* data, uint32_t size,
				object layoutDesc, uint64_t flags) override {
				auto layout = materializeLayout(layoutDesc);
				return toHandle(bgfx::createVertexBuffer(
					mem(data, size), layout, flags));
			}
			renderHandle createDynamicVertexBuffer(const void* data,
				uint32_t size, object layoutDesc,
				uint64_t flags) override {
				auto layout = materializeLayout(layoutDesc);
				return toHandle(bgfx::createDynamicVertexBuffer(
					mem(data, size), layout, flags));
			}
			renderHandle createIndexBuffer(const void* data, uint32_t size,
				uint64_t flags) override {
				return toHandle(bgfx::createIndexBuffer(
					mem(data, size), uint32_t(flags)));
			}
			renderHandle createDynamicIndexBuffer(const void* data,
				uint32_t size, uint64_t flags) override {
				return toHandle(bgfx::createDynamicIndexBuffer(
					mem(data, size), uint32_t(flags)));
			}
			renderHandle createTransientVertexBuffer(object layoutDesc,
				uint16_t count) override {
				bgfx::TransientVertexBuffer* tvb =
					new bgfx::TransientVertexBuffer();
				allocTransientVertexBuffer(tvb, count,
					materializeLayout(layoutDesc));
				renderHandle h{uint16_t(transientVBs.size() + 1)};  // 1-based id
				transientVBs[h] = std::unique_ptr<bgfx::TransientVertexBuffer>(tvb);
				return h;
			}
			renderHandle createTransientIndexBuffer(uint16_t count) override {
				bgfx::TransientIndexBuffer* tib =
					new bgfx::TransientIndexBuffer();
				allocTransientIndexBuffer(tib, count,
					uint32_t(BGFX_BUFFER_INDEX32));
				renderHandle h{uint16_t(transientIBs.size() + 1)};
				transientIBs[h] =
					std::unique_ptr<bgfx::TransientIndexBuffer>(tib);
				return h;
			}
			void updateVertexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				auto it = transientVBs.find(h);
				if (it != transientVBs.end()) {
					auto* dst =
						(uint8_t*)it->second->data + start;
					memcpy(dst, data, size);
					return;
				}
				bgfx::DynamicVertexBufferHandle dh{h.idx};
				bgfx::update(dh, start, mem(data, size));
			}
			void updateIndexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				auto it = transientIBs.find(h);
				if (it != transientIBs.end()) {
					auto* dst = (uint8_t*)it->second->data + start;
					memcpy(dst, data, size);
					return;
				}
				bgfx::DynamicIndexBufferHandle dh{h.idx};
				bgfx::update(dh, start, mem(data, size));
			}
			// gold's 0 `num` (vertices or indices) = the whole buffer;
			// bgfx's encoder default is UINT32_MAX for the same.
			void setVertexBuffer(uint8_t stream, renderHandle h,
				uint32_t start, uint32_t num) override {
				const uint32_t count = num == 0 ? UINT32_MAX : num;
				auto it = transientVBs.find(h);
				if (it != transientVBs.end()) {
					bgfx::setVertexBuffer(stream, it->second.get(), start,
						count);
					return;
				}
				bgfx::setVertexBuffer(stream, vb(h), start, count);
			}
			void setIndexBuffer(renderHandle h, uint32_t start,
				uint32_t num) override {
				const uint32_t count = num == 0 ? UINT32_MAX : num;
				auto it = transientIBs.find(h);
				if (it != transientIBs.end()) {
					bgfx::setIndexBuffer(it->second.get(), start, count);
					return;
				}
				bgfx::setIndexBuffer(ib(h), start, count);
			}
			void destroyBuffer(renderHandle h) override {
				transientVBs.erase(h);
				transientIBs.erase(h);
				bgfx::destroy(vb(h));
			}

			void submit(uint8_t view, renderHandle program, uint32_t depth,
				uint16_t flags) override {
				bgfx::submit(view, prog(program), depth, flags);
			}
			void submitQuery(uint8_t view, renderHandle program,
				renderHandle query, uint32_t depth, uint16_t flags) override {
				bgfx::submit(view, prog(program), oq(query), depth, flags);
			}
			void submitIndirect(uint8_t view, renderHandle program,
				renderHandle indirect, uint16_t start, uint16_t num,
				uint32_t depth, uint16_t flags) override {
				bgfx::submit(view, prog(program), ibuf(indirect), start, num,
					depth, flags);
			}
			void dispatch(uint8_t view, renderHandle program, uint32_t numX,
				uint32_t numY, uint32_t numZ, uint16_t flags) override {
				bgfx::dispatch(view, prog(program), numX, numY, numZ, flags);
			}
			void dispatchIndirect(uint8_t view, renderHandle program,
				renderHandle indirect, uint16_t start, uint16_t num,
				uint16_t flags) override {
				bgfx::dispatch(view, prog(program), ibuf(indirect), start, num,
					flags);
			}

			void setState(uint64_t state, uint32_t rgba) override {
				bgfx::setState(fromGoldState(state), rgba);
			}
			void setStencil(uint32_t fstencil, uint32_t bstencil) override {
				bgfx::setStencil(
					fromGoldStencil(fstencil), fromGoldStencil(bstencil));
			}
			void setTransform(const void* mtx) override {
				bgfx::setTransform(mtx);
			}
			// Binding through a sampler: 0 means "leave the sampler modes
			// as set at creation" (bgfx's UINT32_MAX convention).
			void setTexture(uint8_t stage, const char* sampler,
				renderHandle tex_, uint32_t flags) override {
				bgfx::UniformHandle uh = bgfx::createUniform(
					sampler, bgfx::UniformType::Sampler, 1);
				bgfx::setTexture(stage, uh, tex(tex_),
					flags == 0 ? UINT32_MAX : fromGoldSampler(flags));
				bgfx::destroy(uh);
			}
			void setTextureUniform(uint8_t stage, renderHandle uniform,
				renderHandle tex_, uint32_t flags) override {
				bgfx::setTexture(stage, uni(uniform), tex(tex_),
					flags == 0 ? UINT32_MAX : fromGoldSampler(flags));
			}

			
			void destroyUniform(renderHandle h) override {
				bgfx::destroy(uni(h));
			}
			void setObjectName(renderHandle h, const char* name) override {
				bgfx::setName(tex(h), name, int32_t(strlen(name)));
			}
			bool homogeneousDepth() const override {
				return bgfx::getCaps()->homogeneousDepth;
			}

			renderHandle createFrameBuffer(const void* handles,
				uint8_t num) override {
				return toHandle(bgfx::createFrameBuffer(
					(bgfx::TextureHandle*)handles, num, true));
			}
			renderHandle createFrameBufferSize(uint16_t w, uint16_t h,
				texFormat f, uint64_t flags) override {
				return toHandle(bgfx::createFrameBuffer(
					w, h, toBGFX(f), flags));
			}
			renderHandle createFrameBuffer(object config) override {
				return createFrameBufferFromDescriptor(config);
			}
			renderHandle createFrameBufferFromDescriptor(object config) {
				// The descriptor shapes are the frameBuffer facade's config
				// ones: {"attachments", [{texture, access, layer, mip,
				// resolve}]}, {"textures", [gpuTexture data]}, {"ratio", n,
				// "format", f}, {"nwh", ptr + width/height ("size") +
				// {"color", "depth"}}, and the plain {"width"/"height"
				// ("size"), "format"} — each with an optional
				// {"destroyTextures", bool}.
				const bool destroyTexs = config.getBool("destroyTextures");
				if (config.getType("attachments") == typeList) {
					std::vector<bgfx::Attachment> attachments;
					auto entries = config.getList("attachments");
					for (auto& entry : entries) {
						auto attachment = entry.getObject();
						auto texItem = attachment.getObject("texture");
						auto texHandle = bgfx::TextureHandle{
							texItem.getUInt16(
								"idx", uint16_t(bgfx::kInvalidHandle))};
						bgfx::Attachment att;
						att.init(texHandle,
							bgfx::Access::Enum(attachment.getUInt8(
								"access", uint8_t(bgfx::Access::Write))),
							attachment.getUInt16("layer", 0),
							attachment.getUInt16("mip", 0),
							attachment.getUInt8("resolve", 1));
						attachments.push_back(att);
					}
					return toHandle(bgfx::createFrameBuffer(
						uint8_t(attachments.size()), attachments.data(),
						destroyTexs));
				}
				if (config.getType("textures") == typeList) {
					std::vector<bgfx::TextureHandle> texs;
					auto entries = config.getList("textures");
					for (auto& entry : entries)
						texs.push_back(bgfx::TextureHandle{
							entry.getObject().getUInt16("idx")});
					return toHandle(bgfx::createFrameBuffer(
						uint8_t(texs.size()), texs.data(), destroyTexs));
				}
				auto ratioVar = config.getVar("ratio");
				if (ratioVar.isNumber())
					return toHandle(bgfx::createFrameBuffer(
						bgfx::BackbufferRatio::Enum(ratioVar.getUInt8()),
						bgfx::TextureFormat::Enum(config.getUInt16(
							"format", uint16_t(bgfx::TextureFormat::Count))),
						destroyTexs));
				auto* nwh = (void*)config.getPtr("nwh");
				if (nwh) {
					auto width = config.getUInt16("width");
					auto height = config.getUInt16("height");
					auto size = config.getVar("size");
					if (size.isVec2()) {
						width = size.getUInt16(0);
						height = size.getUInt16(1);
					}
					return toHandle(bgfx::createFrameBuffer(nwh, width,
						height,
						bgfx::TextureFormat::Enum(config.getUInt16(
							"color", uint16_t(bgfx::TextureFormat::Count))),
						bgfx::TextureFormat::Enum(config.getUInt16(
							"depth", uint16_t(bgfx::TextureFormat::Count)))));
				}
				auto width = config.getUInt16("width");
				auto height = config.getUInt16("height");
				auto size = config.getVar("size");
				if (size.isVec2()) {
					width = size.getUInt16(0);
					height = size.getUInt16(1);
				}
				if (width == 0 || height == 0) return renderHandle{};
				return toHandle(bgfx::createFrameBuffer(width, height,
					bgfx::TextureFormat::Enum(config.getUInt16(
						"format", uint16_t(bgfx::TextureFormat::Count))),
					destroyTexs));
			}

			renderHandle createOcclusionQuery() override {
				return toHandle(bgfx::createOcclusionQuery());
			}
			queryResult getQueryResult(renderHandle h,
				int32_t* result) override {
				return (queryResult)uint8_t(
					bgfx::getResult(oq(h), result));
			}
			void setCondition(renderHandle h, bool visible) override {
				bgfx::setCondition(oq(h), visible);
			}
			renderHandle createIndirectBuffer(const void*,
				uint32_t size) override {
				return toHandle(
					bgfx::createIndirectBuffer(size));
			}
			void setViewFrameBuffer(uint8_t view, renderHandle h) override {
				bgfx::setViewFrameBuffer(view, fb(h));
			}
			renderHandle getTexture(renderHandle h,
				uint8_t attachment) override {
				return toHandle(bgfx::getTexture(fb(h), attachment));
			}
			void requestScreenShot(renderHandle h,
				const char* path) override {
				bgfx::requestScreenShot(fb(h), path);
			}
			void setDebug(bool debug, bool stats) override {
				bgfx::setDebug(
					(debug ? BGFX_DEBUG_TEXT : 0) |
					(stats ? BGFX_DEBUG_STATS : 0));
			}
			void touch(uint8_t view) override { bgfx::touch(view); }
			void blit(uint8_t view, renderHandle dst, uint8_t dstMip,
				uint16_t dstX, uint16_t dstY, uint16_t dstZ, renderHandle src,
				uint8_t srcMip, uint16_t srcX, uint16_t srcY, uint16_t srcZ,
				uint16_t w, uint16_t h, uint16_t d) override {
				bgfx::TextureRegion dstRegion;
				dstRegion.handle = tex(dst);
				dstRegion.mip = dstMip;
				dstRegion.x = dstX;
				dstRegion.y = dstY;
				dstRegion.z = dstZ;
				dstRegion.width = w;
				dstRegion.height = h;
				dstRegion.depth = d;
				bgfx::TextureRegion srcRegion;
				srcRegion.handle = tex(src);
				srcRegion.mip = srcMip;
				srcRegion.x = srcX;
				srcRegion.y = srcY;
				srcRegion.z = srcZ;
				srcRegion.width = w;
				srcRegion.height = h;
				srcRegion.depth = d;
				bgfx::blit(view, dstRegion, srcRegion);
			}
			void setImage(uint8_t stage, renderHandle tex_, uint8_t mip,
				texAccess access, texFormat f) override {
				bgfx::setImage(stage, tex(tex_), mip,
					bgfx::Access::Enum(uint8_t(access)),
					toBGFX(f));
			}
		};

		struct bgfxRegistrar {
			bgfxRegistrar() {
				registerRenderBackend(renderBackendType::BGFX,
					[]() -> renderBackend* { return new bgfxRenderBackend(); });
				registerRenderBackendName("bgfx",
					[]() -> renderBackend* { return new bgfxRenderBackend(); });
			}
		};
		bgfxRegistrar bgfxReg;
	}  // namespace

}  // namespace gold
