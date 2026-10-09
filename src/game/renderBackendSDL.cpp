#include "game/renderBackend.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <cstring>

namespace gold {

	namespace {

		// SDL3 GPU (SDL_GPU) render backend. A minimal but working
		// integration: creates a GPU device, claims the SDL window,
		// acquires the swapchain, and presents a cleared frame each
		// beginFrame/endFrame pair. The resource/draw surface of
		// renderBackend is stubbed out (returns invalid handles / no-ops)
		// pending the full SDL_GPU pipeline work.
		class sdlGpuBackend : public renderBackend {
			SDL_GPUDevice* _device = nullptr;
			SDL_Window* _window = nullptr;
			SDL_GPUTextureFormat _format = SDL_GPU_TEXTUREFORMAT_INVALID;
			uint32_t _clearColor = 0;
			uint16_t _width = 0;
			uint16_t _height = 0;
			uint16_t _viewX = 0;
			uint16_t _viewY = 0;
			uint16_t _viewW = 0;
			uint16_t _viewH = 0;
			uint16_t _clearFlags = ClearColor;

			static uint32_t rgbaFloat(float r, float g, float b, float a) {
				return (uint32_t(uint8_t(r * 255)) << 24) |
					(uint32_t(uint8_t(g * 255)) << 16) |
					(uint32_t(uint8_t(b * 255)) << 8) |
					uint32_t(uint8_t(a * 255));
			}

		 public:
			renderBackendType type() const override {
				return renderBackendType::SDLGPU;
			}
			const char* name() const override { return "sdlgpu"; }

			bool initialize(nativeWindow nw, object config) override {
				if (_device) return true;
				_window = (SDL_Window*)nw.window;
				if (!_window) {
					// Headless/offscreen GPU is not supported yet.
					return false;
				}
				_device = SDL_CreateGPUDevice(
					SDL_GPU_SHADERFORMAT_SPIRV |
					SDL_GPU_SHADERFORMAT_MSL |
					SDL_GPU_SHADERFORMAT_DXIL,
					false, nullptr);
				if (!_device) {
					fprintf(stderr, "[SDL3 gpu] %s\n", SDL_GetError());
					return false;
				}
				if (!SDL_ClaimWindowForGPUDevice(_device, _window)) {
					fprintf(stderr, "[SDL3 gpu] %s\n", SDL_GetError());
					SDL_DestroyGPUDevice(_device);
					_device = nullptr;
					return false;
				}
				_format = SDL_GetGPUSwapchainTextureFormat(_device, _window);
				_width = config.getUInt16("width", 1360);
				_height = config.getUInt16("height", 800);
				_viewW = _width;
				_viewH = _height;
				_clearColor = config.getUInt32("rgba", 0x6ab0deff);
				return true;
			}

			void destroy() override {
				if (_device) {
					SDL_ReleaseWindowFromGPUDevice(_device, _window);
					SDL_DestroyGPUDevice(_device);
					_device = nullptr;
				}
				_window = nullptr;
			}

			bool isValid() const override { return _device != nullptr; }

			bool beginFrame() override {
				return _device != nullptr;
			}

			bool endFrame() override {
				if (!_device) return false;
				SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(_device);
				if (!cmd) return false;
				SDL_GPUTexture* swapchain = nullptr;
				if (!SDL_WaitAndAcquireGPUSwapchainTexture(
						cmd, _window, &swapchain, nullptr, nullptr)) {
					SDL_SubmitGPUCommandBuffer(cmd);
					return false;
				}
				if (swapchain) {
					SDL_GPUColorTargetInfo target = {};
					target.texture = swapchain;
					target.clear_color.r =
						float((_clearColor >> 24) & 0xFF) / 255.0f;
					target.clear_color.g =
						float((_clearColor >> 16) & 0xFF) / 255.0f;
					target.clear_color.b =
						float((_clearColor >> 8) & 0xFF) / 255.0f;
					target.clear_color.a =
						float(_clearColor & 0xFF) / 255.0f;
					target.load_op =
						(_clearFlags & ClearColor)
						? SDL_GPU_LOADOP_CLEAR
						: SDL_GPU_LOADOP_LOAD;
					target.store_op = SDL_GPU_STOREOP_STORE;

					SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
						cmd, &target, 1, nullptr);
					if (pass) SDL_EndGPURenderPass(pass);
				}
				return SDL_SubmitGPUCommandBuffer(cmd);
			}

			// ---- views ----------------------------------------------------
			void viewRect(uint8_t view, uint16_t x, uint16_t y,
				uint16_t w, uint16_t h) override {
				(void)view;
				_viewX = x;
				_viewY = y;
				_viewW = w;
				_viewH = h;
			}
			void viewClear(uint8_t view, uint16_t flags, uint32_t rgba,
				float depth, uint8_t stencil) override {
				(void)view;
				(void)depth;
				(void)stencil;
				_clearFlags = flags;
				_clearColor = rgba;
			}
			void viewTransform(uint8_t view, const void* viewMtx,
				const void* projMtx) override {
				(void)view;
				(void)viewMtx;
				(void)projMtx;
			}

			// ---- stubs: full SDL_GPU pipeline pending --------------------
			renderHandle createShader(const void*, uint32_t) override {
				return renderHandle{};
			}
			renderHandle createProgram(renderHandle, renderHandle) override {
				return renderHandle{};
			}
			renderHandle createUniform(const char*, renderUniformType,
				uint16_t) override {
				return renderHandle{};
			}
			void setUniform(renderHandle, const void*, uint16_t) override {}
			void getUniformInfo(renderHandle, string&, renderUniformType&)
				const override {}
			void setShaderUniforms(renderHandle, vector<string>&) override {}

			renderHandle createTexture2D(uint16_t, uint16_t, bool, uint16_t,
				texFormat, uint64_t, const void*, uint32_t) override {
				return renderHandle{};
			}
			renderHandle createTextureCube(uint16_t, bool, uint16_t,
				texFormat, uint64_t, const void*, uint32_t) override {
				return renderHandle{};
			}
			void updateTexture(renderHandle, uint8_t, uint8_t, const void*,
				uint32_t) override {}
			void destroyTexture(renderHandle) override {}
			bool readTexture(renderHandle, void*, uint8_t) override {
				return false;
			}
			void* directAccessPtr(renderHandle) override { return nullptr; }

			renderHandle createVertexBuffer(const void*, uint32_t, object,
				uint64_t) override {
				return renderHandle{};
			}
			renderHandle createDynamicVertexBuffer(const void*, uint32_t,
				object, uint64_t) override {
				return renderHandle{};
			}
			renderHandle createIndexBuffer(const void*, uint32_t,
				uint64_t) override {
				return renderHandle{};
			}
			renderHandle createDynamicIndexBuffer(const void*, uint32_t,
				uint64_t) override {
				return renderHandle{};
			}
			void updateVertexBuffer(renderHandle, const void*, uint32_t,
				uint32_t, uint32_t) override {}
			void updateIndexBuffer(renderHandle, const void*, uint32_t,
				uint32_t, uint32_t) override {}
			void setVertexBuffer(uint8_t, renderHandle, uint32_t,
				uint32_t) override {}
			void setIndexBuffer(renderHandle, uint32_t, uint32_t) override {}
			void destroyBuffer(renderHandle) override {}

			void submit(uint8_t, renderHandle, uint32_t, uint16_t) override {}
			void submitQuery(uint8_t, renderHandle, renderHandle, uint32_t,
				uint16_t) override {}
			void submitIndirect(uint8_t, renderHandle, renderHandle, uint16_t,
				uint16_t, uint32_t, uint16_t) override {}
			void dispatch(uint8_t, renderHandle, uint32_t, uint32_t, uint32_t,
				uint16_t) override {}
			void dispatchIndirect(uint8_t, renderHandle, renderHandle,
				uint16_t, uint16_t, uint16_t) override {}

			void setState(uint64_t, uint32_t) override {}
			void setStencil(uint32_t, uint32_t) override {}
			void setTransform(const void*) override {}
			void setTexture(uint8_t, const char*, renderHandle,
				uint32_t) override {}

			renderHandle createFrameBuffer(const void*, uint8_t) override {
				return renderHandle{};
			}
			renderHandle createFrameBufferSize(uint16_t, uint16_t, texFormat,
				uint64_t) override {
				return renderHandle{};
			}
			renderHandle createOcclusionQuery() override {
				return renderHandle{};
			}
			queryResult getQueryResult(renderHandle, int32_t*) override {
				return queryResult::NoResult;
			}
			void setCondition(renderHandle, bool) override {}
			renderHandle createIndirectBuffer(const void*, uint32_t) override {
				return renderHandle{};
			}
			void setViewFrameBuffer(uint8_t, renderHandle) override {}
			renderHandle getTexture(renderHandle, uint8_t) override {
				return renderHandle{};
			}
			void requestScreenShot(renderHandle, const char*) override {}
			void setDebug(bool, bool) override {}
			void touch(uint8_t) override {}
			void blit(uint8_t, renderHandle, uint8_t, uint16_t, uint16_t,
				uint16_t, renderHandle, uint8_t, uint16_t, uint16_t, uint16_t,
				uint16_t, uint16_t, uint16_t) override {}
			void setImage(uint8_t, renderHandle, uint8_t, texAccess,
				texFormat) override {}
		};

		struct sdlRegistrar {
			sdlRegistrar() {
				registerRenderBackend(renderBackendType::SDLGPU,
					[]() -> renderBackend* {
						return new sdlGpuBackend();
					});
				registerRenderBackendName("sdlgpu",
					[]() -> renderBackend* {
						return new sdlGpuBackend();
					});
				registerRenderBackendName("sdl",
					[]() -> renderBackend* {
						return new sdlGpuBackend();
					});
			}
		};
		sdlRegistrar sdlReg;

	}  // namespace

}  // namespace gold