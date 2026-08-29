#include "game/renderBackend.hpp"

#include <bgfx/bgfx.h>
#if __has_include(<bgfx/platform.h>)
#include <bgfx/platform.h>
#endif

#include <cstring>
#include <map>
#include <mutex>

namespace gold {

	namespace {

		// Map a gold-native texFormat to a bgfx TextureFormat.
		bgfx::TextureFormat::Enum toBGFX(texFormat f) {
			return (bgfx::TextureFormat::Enum)uint8_t(f);
		}
		renderUniformType fromBGFXUniform(bgfx::UniformType::Enum t) {
			return (renderUniformType)uint8_t(t);
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
				return toHandle(bgfx::createUniform(
					name, bgfx::UniformType::Enum(uint8_t(t)), num));
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

}  // namespace gold
