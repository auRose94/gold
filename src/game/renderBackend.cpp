#include "game/renderBackend.hpp"

#include <bgfx/bgfx.h>
#if __has_include(<bgfx/platform.h>)
#include <bgfx/platform.h>
#endif

#include <bx/allocator.h>
#include <bx/readerwriter.h>
#include <bimg/bimg.h>
#include <bimg/encode.h>

#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>

#include "plugin.hpp"

namespace gold {

	namespace {

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
				init.type = bgfx::RendererType::Enum(
					config.getUInt16(
						"rendererType",
						uint16_t(bgfx::RendererType::OpenGL)));
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
				return toHandle(bgfx::createShader(mem(data, size)));
			}
			renderHandle createProgram(renderHandle vs,
				renderHandle fs) override {
				return toHandle(bgfx::createProgram(sh(vs), sh(fs)));
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
					numLayers, toBGFX(f), flags, mem(data, size)));
			}
			renderHandle createTextureCube(uint16_t size, bool hasMips,
				uint16_t numLayers, texFormat f, uint64_t flags,
				const void* data, uint32_t size_) override {
				return toHandle(bgfx::createTextureCube(size, hasMips,
					numLayers, toBGFX(f), flags, mem(data, size_)));
			}
			renderHandle createTexture3D(uint16_t w, uint16_t h, uint16_t d,
				bool hasMips, texFormat f, uint64_t flags, const void* data,
				uint32_t size) override {
				// Volume textures; the whole volume ships with the data.
				return toHandle(bgfx::createTexture3D(
					w, h, d, hasMips, toBGFX(f), flags, mem(data, size)));
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

			renderHandle createVertexBuffer(const void* data, uint32_t size,
				const void* layout) override {
				return toHandle(bgfx::createVertexBuffer(
					mem(data, size), *(const bgfx::VertexLayout*)layout));
			}
			renderHandle createDynamicVertexBuffer(uint32_t size,
				const void* layout) override {
				return renderHandle{uint16_t(bgfx::createDynamicVertexBuffer(
					size, *(const bgfx::VertexLayout*)layout).idx)};
			}
			renderHandle createIndexBuffer(const void* data,
				uint32_t size) override {
				return toHandle(bgfx::createIndexBuffer(
					mem(data, size), BGFX_BUFFER_INDEX32));
			}
			renderHandle createDynamicIndexBuffer(uint32_t size) override {
				return renderHandle{uint16_t(bgfx::createDynamicIndexBuffer(
					size, BGFX_BUFFER_INDEX32).idx)};
			}
			void updateVertexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				bgfx::DynamicVertexBufferHandle dh{h.idx};
				bgfx::update(dh, start, mem(data, size));
			}
			void updateIndexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				bgfx::DynamicIndexBufferHandle dh{h.idx};
				bgfx::update(dh, start, mem(data, size));
			}
			void setVertexBuffer(uint8_t stream, renderHandle h,
				uint32_t start, uint32_t num, const void*) override {
				bgfx::setVertexBuffer(stream, vb(h), start, num);
			}
			void setIndexBuffer(renderHandle h, uint32_t start,
				uint32_t num) override {
				bgfx::setIndexBuffer(ib(h), start, num);
			}
			void destroyBuffer(renderHandle h) override {
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
				bgfx::setState(state, rgba);
			}
			void setStencil(uint32_t fstencil, uint32_t bstencil) override {
				bgfx::setStencil(fstencil, bstencil);
			}
			void setTransform(const void* mtx) override {
				bgfx::setTransform(mtx);
			}
			void setTexture(uint8_t stage, const char* sampler,
				renderHandle tex_, uint32_t flags) override {
				bgfx::UniformHandle uh = bgfx::createUniform(
					sampler, bgfx::UniformType::Sampler, 1);
				bgfx::setTexture(stage, uh, tex(tex_), flags);
				bgfx::destroy(uh);
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

		std::mutex& registryMutex() {
			static std::mutex m;
			return m;
		}
		std::map<renderBackendType, renderBackend* (*)()>& factories() {
			static std::map<renderBackendType, renderBackend* (*)()> f;
			return f;
		}
		std::map<std::string, renderBackend* (*)()>& namedFactories() {
			static std::map<std::string, renderBackend* (*)()> f;
			return f;
		}
	}  // namespace

	void registerRenderBackend(renderBackendType type,
		renderBackend* (*factory)()) {
		std::lock_guard<std::mutex> guard(registryMutex());
		factories()[type] = factory;
	}

	renderBackend* createRenderBackend(renderBackendType type) {
		std::lock_guard<std::mutex> guard(registryMutex());
		auto it = factories().find(type);
		if (it != factories().end()) return it->second();
		return nullptr;
	}

	void registerRenderBackendName(const std::string& name,
		renderBackend* (*factory)()) {
		std::lock_guard<std::mutex> guard(registryMutex());
		namedFactories()[name] = factory;
	}

	renderBackend* createRenderBackend(const std::string& name) {
		plugin::addModulePath((void*)registerRenderBackendName);
		{
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = namedFactories().find(name);
			if (it != namedFactories().end()) return it->second();
		}
		// Miss: a render plugin may provide it (self-registers on load);
		// try each candidate. Loading runs outside the registry mutex.
		for (const auto& candidate : plugin::pluginCandidates(name)) {
			if (!plugin::load(candidate)) continue;
			std::lock_guard<std::mutex> guard(registryMutex());
			auto it = namedFactories().find(name);
			if (it != namedFactories().end()) return it->second();
		}
		return nullptr;
	}

}  // namespace gold