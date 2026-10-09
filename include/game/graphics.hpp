#pragma once

#include "renderBackend.hpp"
#include "types.hpp"
#include "window.hpp"

namespace gold {
	using std::map;
	using std::string;
	class renderBackend;

	/**
	 * Expand a varying definition's #if/#ifdef/#ifndef/#else/#endif
	 * lines against `defines` (the ';'-separated NAME=value list the
	 * shader compile passes). Some shaderc vintages reject directives in
	 * varying definitions; an expanded, directive-free file satisfies
	 * every vintage. Returns the expanded temp file's path.
	 */
	string expandVaryingDefinition(const string& varyingPath,
		const string& defines);

	struct gfxBackend : public object {
	 protected:
		static object& getPrototype();
		static renderBackend*& render();

	 public:
		/**
		 * The active render backend, or null before `initialize()`. Components
		 * that own GPU resources themselves (rather than through a gold
		 * object wrapper) reach the backend through this.
		 */
		static renderBackend* backend();

		gfxBackend();
		gfxBackend(object config);

		var destroy(list args = {});
		var initialize(list args = {});
		var preFrame(list args = {});
		var renderFrame(list args = {});
		var getConfig(list args = {});
		/** Debug helper: {path} — request a PNG of the main back buffer,
		 *  written by the backend's shot callback when the frame ends. */
		var screenshot(list args = {});
	};

	struct frameBuffer : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, frameBuffer> cache;

	 public:
		using backbufferRatio = gold::backbufferRatio;

		frameBuffer();
		frameBuffer(object config);

		void setName(string name);

		var getTexture(uint8_t attachment = 0);

		void setViewFrameBuffer(uint16_t viewId);

		void requestScreenShot(string path);
		void destroy();
	};

	struct occlusionQuery : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, occlusionQuery> cache;

	 public:
		using queryResult = gold::queryResult;

		occlusionQuery();
		occlusionQuery(object config);

		queryResult getResult(int32_t* result = nullptr);
		void setCondition(bool visible);
		void destroy();
	};

	struct indirectBuffer : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, indirectBuffer> cache;

	 public:
		indirectBuffer();
		indirectBuffer(object config);
		void destroy();
	};

	struct shaderObject : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, shaderObject> cache;

	 public:
		shaderObject();
		shaderObject(object config);

		var getAllUniforms(list args = {});
		void destroy();
	};

	using uniformType = renderUniformType;

	struct gpuTexture;
	struct shaderProgram : public object {
	 protected:

		friend gfxBackend;
		static object& getPrototype();
		static map<string, shaderProgram> cache;
		static map<string, object> uniforms;

	 public:
		static shaderProgram findInCache(string name);

		shaderProgram();
		shaderProgram(object config);

		static bool createUniform(
			string name, uniformType t, uint16_t num = 1U);

		static bool setUniform(
			string name, const void* value,
			uint16_t num = 1U);

		static void bindTexture(
			string sampler, uint8_t stage, gpuTexture tex);

		void setState(object state);
		void defaultState();

		void setTransform(var& mtx);

		void setStencil(object state);
		void defaultStencil();

		void setDiscard(string state);
		void defaultDiscard();

		void submit(uint8_t viewId, uint32_t depth = 0);
		void submit(
			uint8_t viewId, occlusionQuery query, uint32_t depth = 0);
		void submit(
			uint8_t viewId, indirectBuffer buffer, uint16_t start = 0,
			uint16_t num = 1, uint32_t depth = 0);

		void dispatch(
			uint8_t viewId, uint32_t numX = 1, uint32_t numY = 1,
			uint32_t numZ = 1);
		void dispatch(
			uint8_t viewId, indirectBuffer buffer, uint16_t start = 0,
			uint16_t num = 1);
		void destroy();
	};

	struct gpuTexture : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, gpuTexture> cache;

		/** The image-container decode path: parse the binary into an
		 *  image, merge its fields, and create the GPU texture. */
		renderHandle parseData(binary& bin);

	 public:
		using textureFormat = texFormat;
		using accessType = texAccess;

		gpuTexture();
		gpuTexture(object config);

		void update(object data);

		void setImage(
			uint8_t stage, uint8_t mip, accessType t,
			textureFormat f = textureFormat::RGBA8);

		void blit(
			uint8_t viewId, var dstP, gpuTexture src, var srcP,
			var size = vec2u16(UINT16_MAX, UINT16_MAX));
		void blit(
			uint8_t viewId, uint8_t dstMip, var dstP, gpuTexture src,
			uint8_t srcMip = 0, var srcP = vec3u16(0, 0, 0),
			var size = vec3u16(UINT16_MAX, UINT16_MAX, UINT16_MAX));

		uint32_t readTexture(void* data, uint8_t mip = 0);
		void* getDirectAccessPtr();
		void setName(string name);
		void destroy();
	};

	struct vertexLayout : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();
		static map<string, vertexLayout> cache;

	 public:
		static vertexLayout findInCache(string name);
		using attrib = vertexAttrib;
		using attribType = vertexAttribType;

		vertexLayout();
		vertexLayout(object config);

		vertexLayout& begin();
		vertexLayout& add(
			attrib att, attribType t, uint8_t count,
			bool norm = false, bool isInt = false);
		vertexLayout& end();
		void destroy();
	};

	enum BufferType {
		nullBufferType = 0,
		standardBufferType = 1,
		dynamicBufferType = 2,
		transientBufferType = 3,
	};

	struct vertexBuffer : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();

	 public:
		vertexBuffer();
		vertexBuffer(object config);

		void set(uint8_t stream);
		void set(
			uint8_t stream, uint32_t start, uint32_t num,
			vertexLayout layout = vertexLayout());

		void update(
			binary data, uint64_t start = 0,
			uint64_t end = UINT64_MAX);

		void destroy();
	};

	struct indexBuffer : public object {
	 protected:
		friend gfxBackend;
		static object& getPrototype();

	 public:
		indexBuffer();
		indexBuffer(object config);

		void set();
		void set(uint32_t start, uint32_t num);

		void update(
			binary data, uint64_t start = 0,
			uint64_t end = UINT64_MAX);

		void destroy();
	};

}  // namespace gold