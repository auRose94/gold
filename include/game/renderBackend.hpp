#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "types.hpp"
#include "game/windowSystem.hpp"

namespace gold {

	/** Renderer family. Maps to a concrete renderBackend implementation. */
	enum class renderBackendType {
		None = 0,
		OpenGL,    // direct GL (not yet implemented)
		Vulkan,    // direct Vulkan (not yet implemented)
		BGFX,      // bgfx renderer (current default)
	};

	/** Opaque GPU object handles. Resources own one of these (an index that
	 * only the active renderBackend understands). */
	struct renderHandle {
		uint16_t idx = 0xFFFF;  // matches bgfx::kInvalidHandle
		bool valid() const { return idx != 0xFFFF; }
	};

	/** Texture pixel formats. A gold-native subset of what the engine uses;
	 * each backend maps these to its own format enum. */
	enum class texFormat : uint8_t {
		Unknown = 0,
		BC1,
		BC2,
		BC3,
		BC4,
		BC5,
		BC6H,
		BC7,
		ETC1,
		ETC2,
		ETC2A,
		ETC2A1,
		PTC12,
		PTC14,
		PTC12A,
		PTC14A,
		PTC22,
		PTC24,
		PTC22A,
		PTC24A,
		ATC,
		ATCE,
		ATCI,
		ASTC4x4,
		ASTC5x5,
		ASTC6x6,
		ASTC8x5,
		ASTC8x6,
		ASTC10x5,
		ASTC10x6,
		ASTC12x6,
		ASTC12x8,
		ASTC12x10,
		ASTC12x12,
		R1,
		A8,
		R8,
		R8I,
		R8U,
		R8S,
		R16,
		R16I,
		R16U,
		R16F,
		R16S,
		R32,
		R32I,
		R32U,
		R32F,
		R32S,
		RG8,
		RG8I,
		RG8U,
		RG8S,
		RG16,
		RG16I,
		RG16U,
		RG16F,
		RG16S,
		RG32,
		RG32I,
		RG32U,
		RG32F,
		RG32S,
		RGB8,
		RGB8I,
		RGB8U,
		RGB8S,
		RGB9E5F,
		BGRA8,
		RGBA8,
		RGBA8I,
		RGBA8U,
		RGBA8S,
		RGBA16,
		RGBA16I,
		RGBA16U,
		RGBA16F,
		RGBA16S,
		RGBA32,
		RGBA32I,
		RGBA32U,
		RGBA32F,
		RGBA32S,
		R5G6B5,
		RGBA4,
		RGB5A1,
		RGB10A2,
		R11G11B10F,
		Count,
	};

	/** Texture sampling access flags (image load/store). */
	enum class texAccess : uint8_t { Read = 0, Write = 1, ReadWrite = 2 };

	/** Uniform data type. */
	enum class renderUniformType : uint8_t {
		Int1 = 0,
		Vec4,
		Mat3,
		Mat4,
		Count,
	};

	/** Vertex attribute semantic. */
	enum class vertexAttrib : uint8_t {
		Position = 0,
		Normal,
		Tangent,
		Bitangent,
		Color0,
		Color1,
		Color2,
		Color3,
		Indices,
		Weight,
		TexCoord0,
		TexCoord1,
		TexCoord2,
		TexCoord3,
		TexCoord4,
		TexCoord5,
		TexCoord6,
		TexCoord7,
		Count,
	};

	/** Vertex attribute storage type. */
	enum class vertexAttribType : uint8_t {
		Uint8 = 0,
		Uint10,
		Int16,
		Float,
		Half,
		Uint8Norm,
		Int16Norm,
		Int32,
		Uint32,
		Count,
	};

	/** Occlusion query result states. */
	enum class queryResult : uint8_t {
		Invisible = 0,
		Visible,
		NoResult,
		Count,
	};

	/** View clear / discard flags (bitwise). */
	enum clearFlags : uint16_t {
		ClearNone = 0,
		ClearColor = 0x0001,
		ClearDepth = 0x0002,
		ClearStencil = 0x0004,
		DiscardColor = 0x0008,
		DiscardDepth = 0x0010,
		DiscardStencil = 0x0020,
	};

	/**
	 * Abstract GPU/rendering backend. Implementations wrap a real API
	 * (bgfx today; Vulkan/OpenGL planned). Handles are opaque uint16
	 * indices owned by this backend; the engine only passes them around.
	 *
	 * The interface intentionally matches how the engine drives rendering:
	 * a frame lifecycle, views with clear+transform+rect, shader programs
	 * with uniforms, vertex/index buffers, textures, and submit/draw calls.
	 */
	class renderBackend {
	 public:
		virtual ~renderBackend() = default;

		/** Backend family. */
		virtual renderBackendType type() const = 0;
		virtual const char* name() const = 0;

		/** Initialize the renderer against a native window (may be null for
		 * headless/offscreen). Returns false on failure. */
		virtual bool initialize(nativeWindow nw, object config) = 0;
		virtual void destroy() = 0;

		/** True while a device is available. */
		virtual bool isValid() const = 0;

		/** Begin/end a frame. endFrame presents. */
		virtual bool beginFrame() = 0;
		virtual bool endFrame() = 0;

		// ---- views ----------------------------------------------------
		virtual void viewRect(uint8_t view, uint16_t x, uint16_t y,
			uint16_t w, uint16_t h) = 0;
		virtual void viewClear(uint8_t view, uint16_t flags,
			uint32_t rgba, float depth, uint8_t stencil) = 0;
		virtual void viewTransform(uint8_t view, const void* viewMtx,
			const void* projMtx) = 0;

		// ---- programs / uniforms --------------------------------------
		virtual renderHandle createShader(const void* data, uint32_t size) = 0;
		virtual renderHandle createProgram(renderHandle vs,
			renderHandle fs) = 0;
		virtual renderHandle createUniform(const char* name,
			renderUniformType t, uint16_t num = 1) = 0;
		virtual void setUniform(renderHandle h, const void* value,
			uint16_t num) = 0;
		virtual void getUniformInfo(renderHandle h, string& name,
			renderUniformType& t) const = 0;
		virtual void setShaderUniforms(renderHandle shader,
			vector<string>& out) = 0;

		// ---- textures ---------------------------------------------------
		virtual renderHandle createTexture2D(uint16_t w, uint16_t h,
			bool hasMips, uint16_t numLayers, texFormat f, uint64_t flags,
			const void* data, uint32_t size) = 0;
		virtual renderHandle createTextureCube(uint16_t size,
			bool hasMips, uint16_t numLayers, texFormat f, uint64_t flags,
			const void* data, uint32_t size_) = 0;
		virtual void updateTexture(renderHandle h, uint8_t side, uint8_t mip,
			const void* data, uint32_t size) = 0;
		virtual void destroyTexture(renderHandle h) = 0;
		virtual bool readTexture(renderHandle h, void* data, uint8_t mip) = 0;
		virtual void* directAccessPtr(renderHandle h) = 0;

		// ---- buffers -----------------------------------------------------
		virtual renderHandle createVertexBuffer(const void* data,
			uint32_t size, const void* layout) = 0;
		virtual renderHandle createDynamicVertexBuffer(uint32_t size,
			const void* layout) = 0;
		virtual renderHandle createIndexBuffer(const void* data,
			uint32_t size) = 0;
		virtual renderHandle createDynamicIndexBuffer(uint32_t size) = 0;
		virtual void updateVertexBuffer(renderHandle h, const void* data,
			uint32_t size, uint32_t start, uint32_t num) = 0;
		virtual void updateIndexBuffer(renderHandle h, const void* data,
			uint32_t size, uint32_t start, uint32_t num) = 0;
		virtual void setVertexBuffer(uint8_t stream, renderHandle h,
			uint32_t start, uint32_t num, const void* layout) = 0;
		virtual void setIndexBuffer(renderHandle h, uint32_t start,
			uint32_t num) = 0;
		virtual void destroyBuffer(renderHandle h) = 0;

		// ---- draw ---------------------------------------------------------
		virtual void submit(uint8_t view, renderHandle program,
			uint32_t depth = 0, uint16_t flags = 0) = 0;
		virtual void submitQuery(uint8_t view, renderHandle program,
			renderHandle query, uint32_t depth = 0,
			uint16_t flags = 0) = 0;
		virtual void submitIndirect(uint8_t view, renderHandle program,
			renderHandle indirect, uint16_t start = 0, uint16_t num = 1,
			uint32_t depth = 0, uint16_t flags = 0) = 0;
		virtual void dispatch(uint8_t view, renderHandle program,
			uint32_t numX = 1, uint32_t numY = 1, uint32_t numZ = 1,
			uint16_t flags = 0) = 0;
		virtual void dispatchIndirect(uint8_t view, renderHandle program,
			renderHandle indirect, uint16_t start = 0, uint16_t num = 1,
			uint16_t flags = 0) = 0;

		// ---- state ---------------------------------------------------------
		virtual void setState(uint64_t state, uint32_t rgba = 0) = 0;
		virtual void setStencil(uint32_t fstencil, uint32_t bstencil) = 0;
		virtual void setTransform(const void* mtx) = 0;
		virtual void setTexture(uint8_t stage, const char* sampler,
			renderHandle tex, uint32_t flags = 0) = 0;

		// ---- framebuffers / queries / misc -------------------------------
		virtual renderHandle createFrameBuffer(const void* handles,
			uint8_t num) = 0;
		virtual renderHandle createFrameBufferSize(uint16_t w, uint16_t h,
			texFormat f, uint64_t flags) = 0;
		virtual renderHandle createOcclusionQuery() = 0;
		virtual queryResult getQueryResult(renderHandle h,
			int32_t* result = nullptr) = 0;
		virtual void setCondition(renderHandle h, bool visible) = 0;
		virtual renderHandle createIndirectBuffer(const void* data,
			uint32_t size) = 0;
		virtual void setViewFrameBuffer(uint8_t view, renderHandle fb) = 0;
		virtual renderHandle getTexture(renderHandle fb,
			uint8_t attachment = 0) = 0;
		virtual void requestScreenShot(renderHandle fb, const char* path) = 0;
		virtual void setDebug(bool debug, bool stats) = 0;
		virtual void touch(uint8_t view) = 0;
		virtual void blit(uint8_t view, renderHandle dst, uint8_t dstMip,
			uint16_t dstX, uint16_t dstY, uint16_t dstZ, renderHandle src,
			uint8_t srcMip, uint16_t srcX, uint16_t srcY, uint16_t srcZ,
			uint16_t w, uint16_t h, uint16_t d) = 0;
		virtual void setImage(uint8_t stage, renderHandle tex, uint8_t mip,
			texAccess access, texFormat f) = 0;
	};

	/** Create a render backend by family. Built-in backends are registered
	 * statically (bgfx in the game module). */
	renderBackend* createRenderBackend(renderBackendType type);
	void registerRenderBackend(renderBackendType type,
		renderBackend* (*factory)());

}  // namespace gold