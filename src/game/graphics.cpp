#include "graphics.hpp"

#include <bgfx/bgfx.h>
#if __has_include(<bgfx/platform.h>)
#include <bgfx/platform.h>
#endif
#include <bimg/bimg.h>
#include <bx/math.h>
#include <bx/os.h>
#ifndef GOLD_USE_SYSTEM_BGFX
#include <brtshaderc.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <file.hpp>
#include <filesystem>
#include <game/renderBackend.hpp>
#include <game/windowSystem.hpp>
#include <image.hpp>
#include <iostream>
#ifdef GOLD_USE_SYSTEM_BGFX
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace gold {
	using namespace std;

#ifdef GOLD_USE_SYSTEM_BGFX
	// Compile a .sc shader with the system bgfx-shaderc tool (dlopen-friendly:
	// the compiler is an external program instead of a statically linked
	// brtshaderc library). The compiled .bin is read back from stdout.
	static const bgfx::Memory* compileShaderSource(
		char type, const char* filePath, const char* defines,
		const char* varyingPath, const char* profile) {
		const char* typeName = type == 'v' ? "vertex"
			: type == 'f' ? "fragment" : "compute";
		string prof = (profile && *profile) ? string(profile)
			: (type == 'c' ? string("430") : string("330"));
		vector<string> args = {
			"/usr/bin/bgfx-shaderc",
			"-f", filePath,
			"--type", typeName,
			"--platform", "linux",
			"--profile", prof,
			"--stdout",
		};
		if (defines && *defines) {
			args.push_back("--define");
			args.push_back(defines);
		}
		if (varyingPath && *varyingPath
			&& filesystem::exists(filesystem::path(varyingPath))) {
			args.push_back("--varyingdef");
			args.push_back(varyingPath);
		}

		vector<char*> argv;
		argv.reserve(args.size() + 1);
		for (auto& a : args) argv.push_back(a.data());
		argv.push_back(nullptr);

		int fds[2];
		if (pipe(fds) != 0) return nullptr;
		pid_t pid = fork();
		if (pid == 0) {
			dup2(fds[1], STDOUT_FILENO);
			close(fds[0]);
			close(fds[1]);
			execvp("/usr/bin/bgfx-shaderc", argv.data());
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
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || out.empty())
			return nullptr;
		return bgfx::copy(out.data(), uint32_t(out.size()));
	}
#endif

	bgfx::PlatformData pd = bgfx::PlatformData();

	renderBackend*& gfxBackend::render() {
		static renderBackend* backend = nullptr;
		return backend;
	}

	map<string, frameBuffer> frameBuffer::cache =
		map<string, frameBuffer>();
	map<string, occlusionQuery> occlusionQuery::cache =
		map<string, occlusionQuery>();
	map<string, indirectBuffer> indirectBuffer::cache =
		map<string, indirectBuffer>();
	map<string, shaderObject> shaderObject::cache =
		map<string, shaderObject>();
	map<string, shaderProgram> shaderProgram::cache =
		map<string, shaderProgram>();
	map<string, gpuTexture> gpuTexture::cache =
		map<string, gpuTexture>();
	map<string, vertexLayout> vertexLayout::cache =
		map<string, vertexLayout>();

	map<string, object> shaderProgram::uniforms =
		map<string, object>();

	obj& gfxBackend::getPrototype() {
		static auto proto = obj({
			{"backend", "OpenGL"},
			{"vSync", true},
			{"maxAnisotropy", false},
			{"stats", false},
			{"debug", false},
			{"window", var()},
			{"initialize", method(&gfxBackend::initialize)},
			{"renderFrame", method(&gfxBackend::renderFrame)},
			{"preFrame", method(&gfxBackend::preFrame)},
			{"destroy", method(&gfxBackend::destroy)},
			{"getConfig", method(&gfxBackend::getConfig)},
		});
		return proto;
	}

	obj defaultBackendConfig = obj({
		{"backend", "OpenGL"},
		{"vSync", true},
		{"maxAnisotropy", false},
		{"stats", false},
		{"debug", false},
	});

	bgfx::RendererType::Enum varToRenderType(var arg) {
		if (arg.isNumber()) {
			return (bgfx::RendererType::Enum)arg.getUInt8();
		} else if (arg.isString()) {
			auto str = string(arg.getString());
			std::transform(
				str.begin(), str.end(), str.begin(),
				[](unsigned char c) { return std::tolower(c); });
			if (str == "noop")
				return bgfx::RendererType::Noop;
			else if (str == "direct3d11")
				return bgfx::RendererType::Direct3D11;
			else if (str == "direct3d12")
				return bgfx::RendererType::Direct3D12;
			else if (str == "gnm")
				return bgfx::RendererType::Gnm;
			else if (str == "metal")
				return bgfx::RendererType::Metal;
			else if (str == "nvn")
				return bgfx::RendererType::Nvn;
			else if (str == "opengles")
				return bgfx::RendererType::OpenGLES;
			else if (str == "opengl")
				return bgfx::RendererType::OpenGL;
			else if (str == "vulkan")
				return bgfx::RendererType::Vulkan;
		}
		return bgfx::RendererType::Noop;
	}

	var gfxBackend::initialize(list args) {
		if (args.size() == 0 || !args[0].isObject())
			return genericError("Graphics initialization requires a window");
		auto win = args[0].getObject<window>();
		setObject("window", win);
		auto ws = win.getBackend();
		nativeWindow nw;
		if (ws) nw = ws->native();

		// A headless window has no drawable to present to, so there is no
		// point in creating a windowed GL context. Use the Noop renderer
		// (offscreen/CI runs) instead of failing at swap time.
		if (!nw.handle && !nw.display && !nw.window)
			setString("backend", "noop");

		// Create the render backend through the renderBackend interface.
		// Defaults to BGFX; a config "renderBackend" of "sdlgpu"/"sdl"
		// selects the SDL3 GPU backend. Falls back to BGFX when the
		// requested backend is unavailable.
		auto backendType = renderBackendType::BGFX;
		auto rb = getString("renderBackend", "bgfx");
		if (rb == "sdlgpu" || rb == "sdl")
			backendType = renderBackendType::SDLGPU;
		else if (rb == "vulkan")
			backendType = renderBackendType::Vulkan;
		auto backend = createRenderBackend(backendType);
		if (!backend) backend = createRenderBackend(renderBackendType::BGFX);
		if (!backend) return genericError("No render backend available");
		render() = backend;

		auto cfg = obj({
			{"rendererType", (uint16_t)varToRenderType(getVar("backend"))},
			{"vSync", getBool("vSync")},
			{"maxAnisotropy", getBool("maxAnisotropy")},
			{"stats", getBool("stats")},
			{"debug", getBool("debug")},
			{"width", win.getUInt32("width")},
			{"height", win.getUInt32("height")},
			{"rgba", win.getUInt32("rgba", 0x6ab0deff)},
		});
		if (!backend->initialize(nw, cfg))
			return genericError("Render backend failed to initialize");
		return var();
	}

	var gfxBackend::renderFrame(list) {
		auto backend = render();
		if (backend) {
			backend->endFrame();
			backend->beginFrame();
		}
		return var();
	}

	var gfxBackend::getConfig(list) {
		auto allowed = defaultBackendConfig;
		auto config = obj(defaultBackendConfig);
		for (auto it = begin(); it != end(); ++it) {
			auto def = allowed[it->first];
			if (def.getType() != typeNull && it->second != def)
				config.setVar(it->first, it->second);
		}
		return config;
	}

	var gfxBackend::destroy(list) {
		for (auto it = frameBuffer::cache.begin();
				 it != frameBuffer::cache.end();
				 ++it)
			it->second.destroy();
		frameBuffer::cache.clear();

		for (auto it = occlusionQuery::cache.begin();
				 it != occlusionQuery::cache.end();
				 ++it)
			it->second.destroy();
		occlusionQuery::cache.clear();

		for (auto it = indirectBuffer::cache.begin();
				 it != indirectBuffer::cache.end();
				 ++it)
			it->second.destroy();
		indirectBuffer::cache.clear();

		for (auto it = shaderObject::cache.begin();
				 it != shaderObject::cache.end();
				 ++it)
			it->second.destroy();
		shaderObject::cache.clear();

		for (auto it = shaderProgram::cache.begin();
				 it != shaderProgram::cache.end();
				 ++it)
			it->second.destroy();
		shaderProgram::cache.clear();

		for (auto it = gpuTexture::cache.begin();
				 it != gpuTexture::cache.end();
				 ++it)
			it->second.destroy();
		gpuTexture::cache.clear();

		for (auto it = vertexLayout::cache.begin();
				 it != vertexLayout::cache.end();
				 ++it)
			it->second.destroy();
		vertexLayout::cache.clear();

		for (auto it = shaderProgram::uniforms.begin();
				 it != shaderProgram::uniforms.end();
				 ++it) {
			auto o = it->second;
			auto uniform = bgfx::UniformHandle{
				o.getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::destroy(uniform);
		}

		if (auto backend = render()) {
			backend->destroy();
			delete backend;
			render() = nullptr;
		}
		empty();
		return var();
	}

	var gfxBackend::preFrame(list) {
		if (auto backend = render()) backend->touch(0);
		return var();
	}

	gfxBackend::gfxBackend() : obj() {}

	gfxBackend::gfxBackend(obj config) : obj(config) {
		setParent(getPrototype());
	}

	object& frameBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	frameBuffer::frameBuffer() : obj() {}
	frameBuffer::frameBuffer(object config) : obj(config) {
		setParent(getPrototype());
		bgfx::FrameBufferHandle handle = {bgfx::kInvalidHandle};
		if (config.getType("attachments") == typeList) {
			auto attachments = vector<bgfx::Attachment>();
			auto attachentObjs = config.getList("attachments");
			auto destroyTexs = config.getBool("destroyTextures");
			for (auto it = attachentObjs.begin();
					 it != attachentObjs.end();
					 ++it) {
				auto attachment = it->getObject();
				auto tex = attachment.getObject<gpuTexture>("texture");
				auto texHandle = bgfx::TextureHandle{
					tex.getUInt16("idx", bgfx::kInvalidHandle)};
				auto access = (bgfx::Access::Enum)attachment.getUInt8(
					"access", bgfx::Access::Write);
				auto layer = attachment.getUInt16("layer", 0);
				auto mip = attachment.getUInt16("mip", 0);
				auto resolve = attachment.getUInt8(
					"resolve", BGFX_RESOLVE_AUTO_GEN_MIPS);
				attachments.push_back(bgfx::Attachment{
					access, texHandle, mip, layer, resolve});
			}

			handle = bgfx::createFrameBuffer(
				uint8_t(attachments.size()), attachments.data(),
				destroyTexs);
		} else if (config.getType("textures") == typeList) {
			auto handles = vector<bgfx::TextureHandle>();
			auto textureObjs = config.getList("textures");
			auto destroyTexs = config.getBool("destroyTextures");
			for (auto it = textureObjs.begin();
					 it != textureObjs.end();
					 ++it) {
				auto tex = it->getObject<gpuTexture>();
				handles.push_back(
					bgfx::TextureHandle{tex.getUInt16("idx")});
			}
			handle = bgfx::createFrameBuffer(
				uint8_t(handles.size()), handles.data(), destroyTexs);
		} else if (config.getVar("ratio").isNumber()) {
			auto ratio =
				(bgfx::BackbufferRatio::Enum)config.getUInt8("ratio");
			auto format = (bgfx::TextureFormat::Enum)config.getUInt16(
				"format", bgfx::TextureFormat::Count);
			auto destroyTexs = config.getBool("destroyTextures");
			handle =
				bgfx::createFrameBuffer(ratio, format, destroyTexs);
		} else if (config.getType("nwh") == typePtr) {
			auto nwh = config.getPtr("nwh");
			auto colFormat =
				(bgfx::TextureFormat::Enum)config.getUInt16(
					"color", bgfx::TextureFormat::Count);
			auto depFormat =
				(bgfx::TextureFormat::Enum)config.getUInt16(
					"depth", bgfx::TextureFormat::Count);
			uint16_t width = 0;
			uint16_t height = 0;
			if (config.getVar("size").isVec2()) {
				auto size = config.getVar("size");
				width = size.getUInt16(0);
				height = size.getUInt16(1);
			} else {
				width = config.getUInt16("width");
				height = config.getUInt16("height");
			}
			handle = bgfx::createFrameBuffer(
				nwh, width, height, colFormat, depFormat);
		} else if (
			config.getVar("size").isVec2() ||
			(config.getVar("width").isNumber() &&
			 config.getVar("height").isNumber())) {
			auto destroyTexs = config.getBool("destroyTextures");
			auto format = (bgfx::TextureFormat::Enum)config.getUInt16(
				"format", bgfx::TextureFormat::Count);
			uint16_t width = 0;
			uint16_t height = 0;
			if (config.getVar("size").isVec2()) {
				auto size = config.getVar("size");
				width = size.getUInt16(0);
				height = size.getUInt16(1);
			} else {
				width = config.getUInt16("width");
				height = config.getUInt16("height");
			}
			handle = bgfx::createFrameBuffer(
				width, height, format, destroyTexs);
		}
		if (handle.idx != bgfx::kInvalidHandle)
			setUInt16("idx", handle.idx);
	}

	void frameBuffer::setName(string name) {
		auto handle = bgfx::FrameBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		bgfx::setName(handle, name.c_str(), int32_t(name.size()));
	}

	var frameBuffer::getTexture(uint8_t attachment) {
		auto handle = bgfx::FrameBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(handle)) return var();
		auto texHandle = bgfx::getTexture(handle, attachment);
		if (!bgfx::isValid(texHandle)) return var();
		return gpuTexture(obj{{"idx", texHandle.idx}});
	}

	void frameBuffer::setViewFrameBuffer(uint16_t viewId) {
		auto handle = bgfx::FrameBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		bgfx::setViewFrameBuffer(viewId, handle);
	}

	void frameBuffer::requestScreenShot(string path) {
		auto handle = bgfx::FrameBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		bgfx::requestScreenShot(handle, path.c_str());
	}

	void frameBuffer::destroy() {
		auto handle = bgfx::FrameBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		empty();
	}

	object& occlusionQuery::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	occlusionQuery::occlusionQuery() : obj() {}
	occlusionQuery::occlusionQuery(object config) : obj(config) {
		setParent(getPrototype());
		auto ocq = bgfx::createOcclusionQuery();
		setUInt16("idx", ocq.idx);
	}

	occlusionQuery::queryResult occlusionQuery::getResult(
		int32_t* result) {
		auto handle = bgfx::OcclusionQueryHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle))
			return (queryResult)bgfx::getResult(handle, result);
		return occlusionQuery::queryResult::NoResult;
	}

	void occlusionQuery::setCondition(bool visible) {
		auto handle = bgfx::OcclusionQueryHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle))
			return bgfx::setCondition(handle, visible);
	}

	void occlusionQuery::destroy() {
		auto handle = bgfx::OcclusionQueryHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		empty();
	}

	object& indirectBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	indirectBuffer::indirectBuffer() : obj() {}

	indirectBuffer::indirectBuffer(object config) : obj(config) {
		setParent(getPrototype());
	}

	void indirectBuffer::destroy() {
		auto handle = bgfx::IndirectBufferHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		empty();
	}

	object& shaderObject::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	shaderObject::shaderObject() : obj() {}

	shaderObject::shaderObject(object config) : obj(config) {
		setParent(getPrototype());
		auto v = getVar("data");
		auto s = getVar("src");
		if (v.isView()) {
			// Load compiled binary
			auto bin = v.getStringView();
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto strData = string_view((char*)mem->data, mem->size);
			auto h = std::hash<string_view>();
			setString("hash", to_string((uint64_t)h(strData)));
			auto handle = bgfx::createShader(mem);
			setUInt16("idx", handle.idx);
		} else if (s.isString()) {
			// Compile from source
#ifdef GOLD_USE_SYSTEM_BGFX
			auto type = char(getUInt8("type", uint8_t('c')));
#else
			auto type = shaderc::ShaderType(
				getUInt8("type", shaderc::ShaderType::ST_COMPUTE));
#endif
			auto path = filesystem::path(s.getString());
			auto defines = getString("defines");
			auto varDef = filesystem::path(path).replace_filename(
				"varying.def.sc");
			auto varying = getString("varying", varDef.string());
			auto inputs = getList("inputs", list());
			auto outputs = getList("outputs", list());
			if (inputs.size() > 0 || outputs.size() > 0) {
				// Generate source from inputs and outputs
				// Prepend i/o arguments for application
				auto source = file::readFile(path).getObject<file>();
				string tempDir =
					filesystem::temp_directory_path().string();
				auto now =
					to_string(duration_cast<std::chrono::milliseconds>(
											std::chrono::high_resolution_clock::now()
												.time_since_epoch())
											.count());
				string tempPath = tempDir + "/shaders/" + now + ".sc";
				filesystem::create_directories(tempDir + "/shaders");
				auto tmpf = fopen(tempPath.c_str(), "w");
				if (!tmpf) {
					empty();
					return;
				}
				// Wrtie inputs
				if (inputs.size() > 0) {
					fputs("$input ", tmpf);
					for (auto it = inputs.begin(); it != inputs.end();
							 ++it) {
						auto item = it->getString() + ", ";
						fputs(item.c_str(), tmpf);
					}
					fputs("\n", tmpf);
				}
				// Write outputs
				if (outputs.size() > 0) {
					fputs("$output ", tmpf);
					for (auto it = outputs.begin(); it != outputs.end();
							 ++it) {
						auto item = it->getString() + ", ";
						fputs(item.c_str(), tmpf);
					}
					fputs("\n", tmpf);
				}
				// Write source
				fputs(string(source).c_str(), tmpf);
				fclose(tmpf);
				path = tempPath;
			}
			// The selected shader compiler supplies the backend profile.
#ifdef GOLD_USE_SYSTEM_BGFX
			auto mem = compileShaderSource(
				type, (const char*)path.c_str(), defines.c_str(),
				varying.c_str(), nullptr);
#else
			auto mem = shaderc::compileShader(
				type, (const char*)path.c_str(), defines.c_str(),
				varying.c_str(), nullptr);
#endif
			if (mem) {
				auto strData = string_view((char*)mem->data, mem->size);
				auto h = std::hash<string_view>();
				setString("hash", to_string((uint64_t)h(strData)));
				auto handle = bgfx::createShader(mem);
				setUInt16("idx", handle.idx);
			} else {
				cerr << "Failed to build: " << path << endl;
				empty();
			}
		}
		if (getType("name") == typeString)
			cache[getString("name")] = *this;
		else
			cache[to_string((uint64_t)this)] = *this;
	}

	var shaderObject::getAllUniforms(list) {
		auto handle = bgfx::ShaderHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) {
			auto count = bgfx::getShaderUniforms(handle);
			auto uniforms = vector<bgfx::UniformHandle>();
			uniforms.resize(
				count, bgfx::UniformHandle{bgfx::kInvalidHandle});
			bgfx::getShaderUniforms(handle, uniforms.data(), count);
			auto uniformData = obj{};
			for (auto it = uniforms.begin(); it != uniforms.end();
					 ++it) {
				auto uniform = *it;
				auto info = bgfx::UniformInfo();
				bgfx::getUniformInfo(uniform, info);
				uniformData.setObject(
					info.name,
					obj{
						{"handle", uniform.idx},
						{"type", info.type},
						{"num", info.num},
					});
			}
			return uniformData;
		}
		return var();
	}

	void shaderObject::destroy() {
		auto handle = bgfx::ShaderHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		empty();
	}

	object& shaderProgram::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	shaderProgram shaderProgram::findInCache(string name) {
		return cache[name];
	}

	shaderProgram::shaderProgram() : obj() {}

	shaderProgram::shaderProgram(object config) : obj(config) {
		setParent(getPrototype());
		if (
			getType("frag") == typeObject &&
			getType("vert") == typeObject) {
			auto frag = getObject<shaderObject>("frag");
			auto vert = getObject<shaderObject>("vert");
			if (frag && vert) {
				auto fHandle = bgfx::ShaderHandle{
					frag.getUInt16("idx", bgfx::kInvalidHandle)};
				auto vHandle = bgfx::ShaderHandle{
					vert.getUInt16("idx", bgfx::kInvalidHandle)};
				auto handle = bgfx::createProgram(vHandle, fHandle);
				setUInt16("idx", handle.idx);
			}
		} else if (getType("comp") == typeObject) {
			auto comp = getObject<shaderObject>("comp");
			if (comp) {
				auto cHandle = bgfx::ShaderHandle{
					comp.getUInt16("idx", bgfx::kInvalidHandle)};
				auto handle = bgfx::createProgram(cHandle);
				setUInt16("idx", handle.idx);
			}
		}
		auto name = getString("name");
		if (name != "") cache[name] = *this;
	}

	bool shaderProgram::createUniform(
		string name, uniformType t, uint16_t num) {
		for (auto it = uniforms.begin(); it != uniforms.end();
				 ++it) {
			auto uniName = it->first;
			if (name.compare(uniName) == 0) {
				return true;
			}
		}
		auto handle = bgfx::createUniform(
			name.c_str(), (bgfx::UniformType::Enum)t, num);
		if (bgfx::isValid(handle))
			uniforms[name] = obj{
				{"idx", handle.idx},
				{"type", t},
				{"num", num},
			};

		return true;
	}

	bool shaderProgram::setUniform(
		string name, const void* value, uint16_t num) {
		for (auto it = uniforms.begin(); it != uniforms.end();
				 ++it) {
			auto uniName = it->first;
			auto uniform = it->second;
			if (name.compare(uniName) == 0) {
				auto handle = bgfx::UniformHandle{
					uniform.getUInt16("idx", bgfx::kInvalidHandle)};
				bgfx::setUniform(handle, value, num);
				return true;
			}
		}
		return false;
	}

	void shaderProgram::bindTexture(
		string sampler, uint8_t stage, gpuTexture tex) {
		if (!tex) return;
		auto texHandle = bgfx::TextureHandle{
			tex.getUInt16("idx", bgfx::kInvalidHandle)};
		auto uniform = uniforms[sampler];
		if (!uniform) return;
		auto uniformHandle = bgfx::UniformHandle{
			uniform.getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(uniformHandle)) return;
		if (!bgfx::isValid(texHandle)) return;
		uint32_t flags = UINT32_MAX;
		bgfx::setTexture(stage, uniformHandle, texHandle, flags);
	}

	inline void toLower(string& str) {
		std::transform(
			str.begin(), str.end(), str.begin(),
			[](unsigned char c) { return std::tolower(c); });
	}

	inline bool exists(string& str, string needle) {
		return str.find(needle) != string::npos;
	}

	inline uint64_t strToBlend(string value) {
		uint64_t a = 0;
		if (exists(value, "zero"))
			a |= BGFX_STATE_BLEND_ZERO;
		else if (exists(value, "one"))
			a |= BGFX_STATE_BLEND_ONE;
		else if (exists(value, "src_color"))
			a |= BGFX_STATE_BLEND_SRC_COLOR;
		else if (exists(value, "inv_src_color"))
			a |= BGFX_STATE_BLEND_INV_SRC_COLOR;
		else if (exists(value, "src_alpha"))
			a |= BGFX_STATE_BLEND_SRC_ALPHA;
		else if (exists(value, "inv_src_alpha"))
			a |= BGFX_STATE_BLEND_INV_SRC_ALPHA;
		else if (exists(value, "dst_alpha"))
			a |= BGFX_STATE_BLEND_DST_ALPHA;
		else if (exists(value, "inv_dst_alpha"))
			a |= BGFX_STATE_BLEND_INV_DST_ALPHA;
		else if (exists(value, "dst_color"))
			a |= BGFX_STATE_BLEND_DST_COLOR;
		else if (exists(value, "inv_dst_color"))
			a |= BGFX_STATE_BLEND_INV_DST_COLOR;
		else if (exists(value, "src_alpha_sat"))
			a |= BGFX_STATE_BLEND_SRC_ALPHA_SAT;
		else if (exists(value, "factor"))
			a |= BGFX_STATE_BLEND_FACTOR;
		else if (exists(value, "inv_factor"))
			a |= BGFX_STATE_BLEND_INV_FACTOR;
		return a;
	}

	void shaderProgram::setState(object state) {
		uint64_t flags = 0;
		if (state.getType("flags") == typeNull) {
			auto write = state.getString("write");
			if (write != "") {
				toLower(write);
				if (exists(write, "r")) flags |= BGFX_STATE_WRITE_R;
				if (exists(write, "g")) flags |= BGFX_STATE_WRITE_G;
				if (exists(write, "b")) flags |= BGFX_STATE_WRITE_B;
				if (exists(write, "a")) flags |= BGFX_STATE_WRITE_A;
				if (exists(write, "z")) flags |= BGFX_STATE_WRITE_Z;
			} else
				flags |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
								 BGFX_STATE_WRITE_Z;

			auto depth = state.getString("depth");
			if (depth != "") {
				toLower(depth);
				if (exists(depth, "less"))
					flags |= BGFX_STATE_DEPTH_TEST_LESS;
				else if (exists(depth, "lequal"))
					flags |= BGFX_STATE_DEPTH_TEST_LEQUAL;
				else if (exists(depth, "equal"))
					flags |= BGFX_STATE_DEPTH_TEST_EQUAL;
				else if (exists(depth, "gequal"))
					flags |= BGFX_STATE_DEPTH_TEST_GEQUAL;
				else if (exists(depth, "greater"))
					flags |= BGFX_STATE_DEPTH_TEST_GREATER;
				else if (exists(depth, "notequal"))
					flags |= BGFX_STATE_DEPTH_TEST_NOTEQUAL;
				else if (exists(depth, "never"))
					flags |= BGFX_STATE_DEPTH_TEST_NEVER;
				else if (exists(depth, "always"))
					flags |= BGFX_STATE_DEPTH_TEST_ALWAYS;
			} else {
				flags |= BGFX_STATE_DEPTH_TEST_LESS;
			}

			auto blend = state.getString("blend");
			auto blendA = state.getString("blendSrc");
			auto blendB = state.getString("blendDst");
			auto blendRGBSrc = state.getString("blendRGBSrc");
			auto blendRGBDst = state.getString("blendRGBDst");
			if (blend != "") {
				toLower(blend);
				if (exists(blend, "independent"))
					flags |= BGFX_STATE_BLEND_INDEPENDENT;
				else if (exists(blend, "alpha_to_coverage"))
					flags |= BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;
				else if (exists(blend, "add"))
					flags |= BGFX_STATE_BLEND_EQUATION_ADD;
				else if (exists(blend, "sub"))
					flags |= BGFX_STATE_BLEND_EQUATION_SUB;
				else if (exists(blend, "revsub"))
					flags |= BGFX_STATE_BLEND_EQUATION_REVSUB;
				else if (exists(blend, "min"))
					flags |= BGFX_STATE_BLEND_EQUATION_MIN;
				else if (exists(blend, "max"))
					flags |= BGFX_STATE_BLEND_EQUATION_MAX;
			} else if (
				blendRGBSrc != "" && blendRGBDst != "" &&
				blendA != "" && blendB != "") {
				uint64_t a = strToBlend(blendRGBSrc);
				uint64_t b = strToBlend(blendRGBDst);
				uint64_t x = strToBlend(blendA);
				uint64_t y = strToBlend(blendB);
				flags |= BGFX_STATE_BLEND_FUNC_SEPARATE(a, b, x, y);
			} else if (blendA != "" && blendB != "") {
				uint64_t a = strToBlend(blendA);
				uint64_t b = strToBlend(blendB);
				flags |= BGFX_STATE_BLEND_FUNC(a, b);
			} else {
				flags |= BGFX_STATE_BLEND_NORMAL;
			}

			auto cull = state.getString("cull");
			if (cull != "") {
				toLower(cull);
				if (exists(cull, "cw"))
					flags |= BGFX_STATE_CULL_CW;
				else if (exists(cull, "ccw"))
					flags |= BGFX_STATE_CULL_CCW;
			} else {
				flags |= BGFX_STATE_CULL_CW;
			}

			if (state.getVar("alphaRef").isNumber()) {
				auto alphaValue = state.getUInt64("alphaRef");
				flags |= BGFX_STATE_ALPHA_REF(alphaValue);
			}

			auto primitiveType = state.getString("type");
			if (primitiveType != "") {
				toLower(primitiveType);
				if (exists(primitiveType, "tristrip"))
					flags |= BGFX_STATE_PT_TRISTRIP;
				else if (exists(primitiveType, "linestrip"))
					flags |= BGFX_STATE_PT_LINESTRIP;
				else if (exists(primitiveType, "lines"))
					flags |= BGFX_STATE_PT_LINES;
				else if (exists(primitiveType, "points"))
					flags |= BGFX_STATE_PT_POINTS;
			} else {
				flags |= BGFX_STATE_PT_TRISTRIP;
			}

			if (state.getVar("pointSize").isNumber()) {
				auto pointSize = state.getUInt64("pointSize");
				flags |= BGFX_STATE_POINT_SIZE(pointSize);
			}

			if (
				state.getType("MSAA") == typeBool &&
				state.getBool("MSAA")) {
				flags |= BGFX_STATE_MSAA;
			}

			if (
				state.getType("lineAA") == typeBool &&
				state.getBool("lineAA")) {
				flags |= BGFX_STATE_LINEAA;
			}

			if (
				state.getType("conservative") == typeBool &&
				state.getBool("conservative")) {
				flags |= BGFX_STATE_CONSERVATIVE_RASTER;
			}

		} else {
			flags = state.getUInt64("flags");
		}
		auto color = state.getUInt32("blendColor");
		setUInt64("state", flags);
		setUInt32("blendColor", color);
		bgfx::setState(flags, color);
	}

	void shaderProgram::defaultState() {
		bgfx::setState(
			0 | BGFX_STATE_BLEND_NORMAL | BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
			BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA |
			BGFX_STATE_CULL_CW);
	}

	void shaderProgram::setTransform(var& mtx) {
		bgfx::setTransform(mtx.getPtr(), 1U);
	}

	uint32_t parseStencil(
		string test, string fS, string fZ, string pZ) {
		uint32_t flags = 0;
		toLower(test);
		toLower(fS);
		toLower(fZ);
		toLower(pZ);

		if (exists(test, "less"))
			flags |= BGFX_STENCIL_TEST_LESS;
		else if (exists(test, "lequal"))
			flags |= BGFX_STENCIL_TEST_LEQUAL;
		else if (exists(test, "equal"))
			flags |= BGFX_STENCIL_TEST_EQUAL;
		else if (exists(test, "gequal"))
			flags |= BGFX_STENCIL_TEST_GEQUAL;
		else if (exists(test, "greater"))
			flags |= BGFX_STENCIL_TEST_GREATER;
		else if (exists(test, "notequal"))
			flags |= BGFX_STENCIL_TEST_NOTEQUAL;
		else if (exists(test, "never"))
			flags |= BGFX_STENCIL_TEST_NEVER;
		else if (exists(test, "never"))
			flags |= BGFX_STENCIL_TEST_NEVER;
		else if (exists(test, "always"))
			flags |= BGFX_STENCIL_TEST_ALWAYS;

		if (exists(fS, "zero"))
			flags |= BGFX_STENCIL_OP_FAIL_S_ZERO;
		else if (exists(fS, "keep"))
			flags |= BGFX_STENCIL_OP_FAIL_S_KEEP;
		else if (exists(fS, "replace"))
			flags |= BGFX_STENCIL_OP_FAIL_S_REPLACE;
		else if (exists(fS, "incr"))
			flags |= BGFX_STENCIL_OP_FAIL_S_INCR;
		else if (exists(fS, "incrsat"))
			flags |= BGFX_STENCIL_OP_FAIL_S_INCRSAT;
		else if (exists(fS, "decr"))
			flags |= BGFX_STENCIL_OP_FAIL_S_DECR;
		else if (exists(fS, "decrsat"))
			flags |= BGFX_STENCIL_OP_FAIL_S_DECRSAT;
		else if (exists(fS, "invert"))
			flags |= BGFX_STENCIL_OP_FAIL_S_INVERT;

		if (exists(fZ, "zero"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_ZERO;
		else if (exists(fZ, "keep"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_KEEP;
		else if (exists(fZ, "replace"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_REPLACE;
		else if (exists(fZ, "incr"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_INCR;
		else if (exists(fZ, "incrsat"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_INCRSAT;
		else if (exists(fZ, "decr"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_DECR;
		else if (exists(fZ, "decrsat"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_DECRSAT;
		else if (exists(fZ, "invert"))
			flags |= BGFX_STENCIL_OP_FAIL_Z_INVERT;

		if (exists(pZ, "zero"))
			flags |= BGFX_STENCIL_OP_PASS_Z_ZERO;
		else if (exists(pZ, "keep"))
			flags |= BGFX_STENCIL_OP_PASS_Z_KEEP;
		else if (exists(pZ, "replace"))
			flags |= BGFX_STENCIL_OP_PASS_Z_REPLACE;
		else if (exists(pZ, "incr"))
			flags |= BGFX_STENCIL_OP_PASS_Z_INCR;
		else if (exists(pZ, "incrsat"))
			flags |= BGFX_STENCIL_OP_PASS_Z_INCRSAT;
		else if (exists(pZ, "decr"))
			flags |= BGFX_STENCIL_OP_PASS_Z_DECR;
		else if (exists(pZ, "decrsat"))
			flags |= BGFX_STENCIL_OP_PASS_Z_DECRSAT;
		else if (exists(pZ, "invert"))
			flags |= BGFX_STENCIL_OP_PASS_Z_INVERT;

		return flags;
	}

	void shaderProgram::setStencil(object state) {
		auto frontStencilOp = state.getString("frontStencilOp");
		auto frontFS = state.getString("frontFailS");
		auto frontFZ = state.getString("frontFailZ");
		auto frontPZ = state.getString("frontPassZ");

		auto backStencilOp = state.getString("backStencilOp");
		auto backFS = state.getString("backFailS");
		auto backFZ = state.getString("backFailZ");
		auto backPZ = state.getString("backPassZ");
		uint32_t fstencil =
			parseStencil(frontStencilOp, frontFS, frontFZ, frontPZ);
		uint32_t bstencil =
			parseStencil(backStencilOp, backFS, backFZ, backPZ);

		bgfx::setStencil(fstencil, bstencil);
	}

	void shaderProgram::defaultStencil() {
		bgfx::setStencil(BGFX_STENCIL_NONE);
	}

	void shaderProgram::setDiscard(string state) {
		toLower(state);
		uint8_t flag = 0;
		if (exists(state, "all"))
			flag |= BGFX_DISCARD_ALL;
		else if (exists(state, "bindings"))
			flag |= BGFX_DISCARD_BINDINGS;
		else if (exists(state, "index_buffer"))
			flag |= BGFX_DISCARD_INDEX_BUFFER;
		else if (exists(state, "instance_data"))
			flag |= BGFX_DISCARD_INSTANCE_DATA;
		else if (exists(state, "state"))
			flag |= BGFX_DISCARD_STATE;
		else if (exists(state, "transform"))
			flag |= BGFX_DISCARD_TRANSFORM;
		else if (exists(state, "vertex_streams"))
			flag |= BGFX_DISCARD_VERTEX_STREAMS;
		setUInt8("discard", flag);
	}
	void shaderProgram::defaultDiscard() { erase("discard"); }

	void shaderProgram::submit(uint8_t viewId, uint32_t depth) {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		uint8_t flags = getUInt8("discard", BGFX_DISCARD_ALL);
		bgfx::submit(viewId, handle, depth, flags);
	}

	void shaderProgram::submit(
		uint8_t viewId, occlusionQuery query, uint32_t depth) {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		auto qHandle = bgfx::OcclusionQueryHandle{
			query.getUInt16("idx", bgfx::kInvalidHandle)};
		uint8_t flags = getUInt8("discard", BGFX_DISCARD_ALL);
		bgfx::submit(viewId, handle, qHandle, depth, flags);
	}
	void shaderProgram::submit(
		uint8_t viewId, indirectBuffer buffer, uint16_t start,
		uint16_t num, uint32_t depth) {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		auto iHandle = bgfx::IndirectBufferHandle{
			buffer.getUInt16("idx", bgfx::kInvalidHandle)};
		uint8_t flags = getUInt8("discard", BGFX_DISCARD_ALL);
		bgfx::submit(
			viewId, handle, iHandle, start, num, depth, flags);
	}

	void shaderProgram::dispatch(
		uint8_t viewId, uint32_t numX, uint32_t numY,
		uint32_t numZ) {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		uint8_t flags = getUInt8("discard", BGFX_DISCARD_ALL);
		bgfx::dispatch(viewId, handle, numX, numY, numZ, flags);
	}

	void shaderProgram::dispatch(
		uint8_t viewId, indirectBuffer buffer, uint16_t start,
		uint16_t num) {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		auto iHandle = bgfx::IndirectBufferHandle{
			buffer.getUInt16("idx", bgfx::kInvalidHandle)};
		uint8_t flags = getUInt8("discard", BGFX_DISCARD_ALL);
		bgfx::dispatch(viewId, handle, iHandle, start, num, flags);
	}

	void shaderProgram::destroy() {
		auto handle = bgfx::ProgramHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		empty();
	}

	object& gpuTexture::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	uint64_t sampleStringToFlags(string value) {
		uint64_t flags = 0;

		if (exists(value, "u_mirror"))
			flags |= BGFX_SAMPLER_U_MIRROR;
		else if (exists(value, "u_clamp"))
			flags |= BGFX_SAMPLER_U_CLAMP;
		else if (exists(value, "u_border"))
			flags |= BGFX_SAMPLER_U_BORDER;

		if (exists(value, "v_mirror"))
			flags |= BGFX_SAMPLER_V_MIRROR;
		else if (exists(value, "v_clamp"))
			flags |= BGFX_SAMPLER_V_CLAMP;
		else if (exists(value, "v_border"))
			flags |= BGFX_SAMPLER_V_BORDER;

		if (exists(value, "w_mirror"))
			flags |= BGFX_SAMPLER_W_MIRROR;
		else if (exists(value, "w_clamp"))
			flags |= BGFX_SAMPLER_W_CLAMP;
		else if (exists(value, "w_border"))
			flags |= BGFX_SAMPLER_W_BORDER;

		if (exists(value, "min_point"))
			flags |= BGFX_SAMPLER_MIN_POINT;
		else if (exists(value, "min_anis"))
			flags |= BGFX_SAMPLER_MIN_ANISOTROPIC;

		if (exists(value, "mag_point"))
			flags |= BGFX_SAMPLER_MAG_POINT;
		else if (exists(value, "mag_anis"))
			flags |= BGFX_SAMPLER_MAG_ANISOTROPIC;

		if (exists(value, "mip_point"))
			flags |= BGFX_SAMPLER_MIP_POINT;

		if (exists(value, "less"))
			flags |= BGFX_SAMPLER_COMPARE_LESS;
		else if (exists(value, "lequal"))
			flags |= BGFX_SAMPLER_COMPARE_LEQUAL;
		else if (exists(value, "gequal"))
			flags |= BGFX_SAMPLER_COMPARE_GEQUAL;
		else if (exists(value, "equal"))
			flags |= BGFX_SAMPLER_COMPARE_EQUAL;
		else if (exists(value, "greater"))
			flags |= BGFX_SAMPLER_COMPARE_GREATER;
		else if (exists(value, "notequal"))
			flags |= BGFX_SAMPLER_COMPARE_NOTEQUAL;
		else if (exists(value, "never"))
			flags |= BGFX_SAMPLER_COMPARE_NEVER;
		else if (exists(value, "always"))
			flags |= BGFX_SAMPLER_COMPARE_ALWAYS;

		return flags;
	}

	bgfx::TextureHandle gpuTexture::parseData(binary& bin) {
		auto flagsStr = getString("flags");
		toLower(flagsStr);
		uint64_t flags = sampleStringToFlags(flagsStr);
		auto handle = bgfx::TextureHandle{bgfx::kInvalidHandle};
		auto img = image({
			{"data", bin},
		});
		copy(img);
		auto pData = getStringView("data");
		auto mem =
			bgfx::makeRef(pData.data(), uint32_t(pData.size()));

		auto format =
			(bgfx::TextureFormat::Enum)getUInt32("format");
		if (getUInt32("depth") > 0) {
			// 3D
			auto width = getUInt16("width");
			auto height = getUInt16("height");
			auto depth = getUInt16("depth");
			auto hasMips = getUInt8("numMips") > 0;
			handle = bgfx::createTexture3D(
				width, height, depth, hasMips, format, flags, mem);
		} else if (getBool("cubeMap")) {
			auto size = getUInt16("width");
			auto numLayers = getUInt16("numLayers");
			// CubeMap
			auto hasMips = getUInt8("numMips") > 0;
			handle = bgfx::createTextureCube(
				size, hasMips, numLayers, format, flags, mem);
		} else {
			// 2D
			auto width = getUInt16("width");
			auto height = getUInt16("height");
			auto numLayers = getUInt16("numLayers");
			auto hasMips = getUInt8("numMips") > 0;
			if (width != 0 && height != 0)
				handle = bgfx::createTexture2D(
					width, height, hasMips, numLayers, format, flags,
					mem);
		}
		return handle;
	}

	gpuTexture::gpuTexture() : obj() { setParent(getPrototype()); }
	gpuTexture::gpuTexture(object config) : obj(config) {
		setParent(getPrototype());
		auto flagsStr = getString("flags");
		toLower(flagsStr);
		uint64_t flags = sampleStringToFlags(flagsStr);
		auto border = config.getVar("border");
		auto name = getString("name");
		if (name == "") name = getString("path");
		if (name == "") name = to_string((uint64_t)this);
		if (border && border.isNumber()) {
			auto color = border.getUInt32();
			flags |= BGFX_SAMPLER_BORDER_COLOR(color);
		}

		auto handle = bgfx::TextureHandle{bgfx::kInvalidHandle};
		auto sizeVar = config.getVar("size");
		auto depthVar = config.getVar("depth");
		auto widthVar = config.getVar("width");
		auto heightVar = config.getVar("height");
		var binData = getVar("data");
		string_view bin;
		if (sizeVar && sizeVar.getUInt16() != 0) {
			bin = binData.getStringView();
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto size = sizeVar.getUInt16();
			auto layers = getUInt16("layers");
			auto mips = getBool("hasMips");
			auto format = (bgfx::TextureFormat::Enum)getUInt32(
				"format", bgfx::TextureFormat::Count);
			handle = bgfx::createTextureCube(
				size, mips, layers, format, flags, mem);
		} else if (
			depthVar && depthVar.getUInt16() != 0 && widthVar &&
			widthVar.getUInt16() != 0 && heightVar &&
			heightVar.getUInt16() != 0) {
			bin = binData.getStringView();
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto width = widthVar.getUInt16();
			auto height = heightVar.getUInt16();
			auto depth = depthVar.getUInt16();
			auto mips = getBool("hasMips");
			auto format = (bgfx::TextureFormat::Enum)getUInt32(
				"format", bgfx::TextureFormat::Count);
			handle = bgfx::createTexture3D(
				width, height, depth, mips, format, flags, mem);
		} else if (
			widthVar && widthVar.getUInt16() != 0 && heightVar &&
			heightVar.getUInt16() != 0) {
			bin = binData.getStringView();
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto width = widthVar.getUInt16();
			auto height = heightVar.getUInt16();
			auto mips = getBool("hasMips");
			auto layers = getUInt16("layers", 0);
			auto format = (bgfx::TextureFormat::Enum)getUInt32(
				"format", bgfx::TextureFormat::Count);
			handle = bgfx::createTexture2D(
				width, height, mips, layers, format, flags, mem);
		} else if (binData.isView()) {
			auto b = binData.getBinary();
			handle = parseData(b);
		} else if (config.getType("path") == typeString) {
			auto path = getString("path");
			if (path.size() > 0) {
				// Load from file, set to object
				auto textRet = file::readFile(path).getObject<file>();
				auto fileData = textRet.getBinary("data");

				handle = parseData(fileData);
			}
		}
		if (bgfx::isValid(handle)) {
			bgfx::setName(handle, name.c_str(), int32_t(name.size()));
			setUInt16("idx", handle.idx);
			cache[name] = *this;
		}
	}

	void gpuTexture::update(object info) {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(handle)) return;
		auto x = getUInt16("x");
		auto y = getUInt16("y");
		auto w = getUInt16("width");
		auto h = getUInt16("height");
		auto mip = getUInt8("mip");
		auto sideVar = info.getVar("side");
		auto depthVar = info.getVar("depth");
		if (sideVar && sideVar.getUInt8() < 6) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto layer = getUInt16("layer");
			auto side = sideVar.getUInt8();
			auto pitch = getUInt16("pitch", UINT16_MAX);
			bgfx::updateTextureCube(
				handle, layer, side, mip, x, y, w, h, mem, pitch);
		} else if (
			depthVar && depthVar.getUInt16() != 0 && w != 0 &&
			h != 0) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto z = getUInt16("z");
			auto depth = depthVar.getUInt16();
			bgfx::updateTexture3D(
				handle, mip, x, y, z, w, h, depth, mem);
		} else if (w != 0 && h != 0) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto layer = getUInt16("layer");
			auto pitch = getUInt16("pitch", UINT16_MAX);
			bgfx::updateTexture2D(
				handle, layer, mip, x, y, w, h, mem, pitch);
		}
	}

	void gpuTexture::setImage(
		uint8_t stage, uint8_t mip, accessType t, textureFormat f) {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(handle)) return;
		bgfx::setImage(
			stage, handle, mip, (bgfx::Access::Enum)t,
			(bgfx::TextureFormat::Enum)f);
	}

	void gpuTexture::blit(
		uint8_t viewId, var dstP, gpuTexture src, var srcP,
		var size) {
		auto dstHandle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		auto srcHandle = bgfx::TextureHandle{
			src.getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(dstHandle)) return;
		if (!bgfx::isValid(srcHandle)) return;
		uint16_t s[2] = {UINT16_MAX, UINT16_MAX};
		if (size.isVec2()) {
			s[0] = size.getUInt16(0);
			s[1] = size.getUInt16(1);
		}
		bgfx::TextureRegion dst;
		dst.handle = dstHandle;
		dst.x = dstP.getUInt16(0);
		dst.y = dstP.getUInt16(1);
		dst.width = s[0];
		dst.height = s[1];
		bgfx::TextureRegion srcRegion;
		srcRegion.handle = srcHandle;
		srcRegion.x = srcP.getUInt16(0);
		srcRegion.y = srcP.getUInt16(1);
		srcRegion.width = s[0];
		srcRegion.height = s[1];
		bgfx::blit(viewId, dst, srcRegion);
	}
	void gpuTexture::blit(
		uint8_t viewId, uint8_t dstMip, var dstP, gpuTexture src,
		uint8_t srcMip, var srcP, var size) {
		auto dstHandle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		auto srcHandle = bgfx::TextureHandle{
			src.getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(dstHandle)) return;
		if (!bgfx::isValid(srcHandle)) return;
		uint16_t s[3] = {UINT16_MAX, UINT16_MAX, UINT16_MAX};
		if (size.isVec3()) {
			s[0] = size.getUInt16(0);
			s[1] = size.getUInt16(1);
			s[2] = size.getUInt16(2);
		}
		bgfx::TextureRegion dst;
		dst.handle = dstHandle;
		dst.mip = dstMip;
		dst.x = dstP.getUInt16(0);
		dst.y = dstP.getUInt16(1);
		dst.z = dstP.getUInt16(2);
		dst.width = s[0];
		dst.height = s[1];
		dst.depth = s[2];
		bgfx::TextureRegion srcRegion;
		srcRegion.handle = srcHandle;
		srcRegion.mip = srcMip;
		srcRegion.x = srcP.getUInt16(0);
		srcRegion.y = srcP.getUInt16(1);
		srcRegion.z = srcP.getUInt16(2);
		srcRegion.width = s[0];
		srcRegion.height = s[1];
		srcRegion.depth = s[2];
		bgfx::blit(viewId, dst, srcRegion);
	}

	uint32_t gpuTexture::readTexture(void* bin, uint8_t mip) {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		bgfx::TextureRegion region;
		region.handle = handle;
		region.mip = mip;
		return bgfx::read(region, bin);
	}
	void* gpuTexture::getDirectAccessPtr() {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(handle)) return nullptr;
		return bgfx::getDirectAccessPtr(handle);
	}
	void gpuTexture::setName(string name) {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (!bgfx::isValid(handle)) return;
		bgfx::setName(handle, name.c_str(), int32_t(name.size()));
	}
	void gpuTexture::destroy() {
		auto handle = bgfx::TextureHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		setUInt16("idx", bgfx::kInvalidHandle);
	}

	object& vertexLayout::getPrototype() {
		static auto proto = obj{
			{"layout", var()},
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	vertexLayout vertexLayout::findInCache(string name) {
		return cache[name];
	}

	vertexLayout::vertexLayout() : obj() {}

	vertexLayout::vertexLayout(object config) : obj(config) {
		setParent(getPrototype());
		auto name = getString("name");
		if (name != "") cache[name] = *this;
	}

	vertexLayout& vertexLayout::begin() {
		auto layout = new bgfx::VertexLayout();
		layout->begin();
		setPtr("layout", layout);
		return *this;
	}

	vertexLayout& vertexLayout::add(
		attrib att, attribType t, uint8_t count, bool norm,
		bool isInt) {
		auto layout = (bgfx::VertexLayout*)getPtr("layout");
		if (layout) {
			layout->add(
				(bgfx::Attrib::Enum)att, count,
				(bgfx::AttribType::Enum)t, norm, isInt);
		}
		return *this;
	}

	vertexLayout& vertexLayout::end() {
		auto layout = (bgfx::VertexLayout*)getPtr("layout");
		if (layout) {
			layout->end();
		}
		return *this;
	}

	void vertexLayout::destroy() {
		auto handle = bgfx::VertexLayoutHandle{
			getUInt16("idx", bgfx::kInvalidHandle)};
		if (bgfx::isValid(handle)) bgfx::destroy(handle);
		auto layout = (bgfx::VertexLayout*)getPtr("layout");
		if (layout) delete layout;
		empty();
	}

	object& vertexBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	vertexBuffer::vertexBuffer() : obj() {}
	vertexBuffer::vertexBuffer(object config) : obj(config) {
		setParent(getPrototype());
		uint16_t flags = 0;
		auto type = getUInt8("type", nullBufferType);
		auto layoutObj = getObject<vertexLayout>("layout");
		auto layout =
			*(bgfx::VertexLayout*)layoutObj.getPtr("layout");
		if (type == standardBufferType) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto handle =
				bgfx::createVertexBuffer(mem, layout, flags);
			setUInt16("idx", handle.idx);
		} else if (type == dynamicBufferType) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto handle =
				bgfx::createDynamicVertexBuffer(mem, layout, flags);
			setUInt16("idx", handle.idx);
		} else if (type == transientBufferType) {
			auto transBuffer = new bgfx::TransientVertexBuffer();
			auto count = getUInt16("count");
			if (count >= 1) {
				bgfx::allocTransientVertexBuffer(
					transBuffer, count, layout);
				setPtr("trans", transBuffer);
			} else
				empty();
		}
	}

	void vertexBuffer::update(
		binary bin, uint64_t start, uint64_t end) {
		auto type = getUInt8("type", nullBufferType);
		auto it = bin.begin();
		if (end == UINT64_MAX) end = bin.size();
		if (type == dynamicBufferType) {
			auto dstBin = getStringView("data");
			auto dstIt = dstBin.begin();
			auto endIt = dstBin.end();
			advance(dstIt, start);
			advance(endIt, end);
			for (; dstIt != endIt; ++dstIt, ++it)
				*((char*)&(*dstIt)) = *it;
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientVertexBuffer*)getPtr("trans");
			auto dstPtr = (uint8_t*)transBuffer->data + start;
			auto endPtr = (uint8_t*)transBuffer->data + end;
			for (; dstPtr != endPtr; ++dstPtr, ++it) {
				*dstPtr = *it;
			}
		}
	}

	void vertexBuffer::set(uint8_t stream) {
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto handle = bgfx::VertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setVertexBuffer(stream, handle);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicVertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setVertexBuffer(stream, handle);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientVertexBuffer*)getPtr("trans");
			bgfx::setVertexBuffer(stream, transBuffer);
		}
	}
	void vertexBuffer::set(
		uint8_t stream, uint32_t start, uint32_t num,
		vertexLayout layoutObj) {
		auto type = getUInt8("type");
		layoutObj =
			layoutObj ? layoutObj : getObject<vertexLayout>("layout");
		auto layout =
			bgfx::VertexLayoutHandle{layoutObj.getUInt16("idx")};
		if (type == standardBufferType) {
			auto handle = bgfx::VertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setVertexBuffer(stream, handle, start, num, layout);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicVertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setVertexBuffer(stream, handle, start, num, layout);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientVertexBuffer*)getPtr("trans");
			bgfx::setVertexBuffer(
				stream, transBuffer, start, num, layout);
		}
	}

	void vertexBuffer::destroy() {
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto handle = bgfx::VertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			if (bgfx::isValid(handle)) bgfx::destroy(handle);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicVertexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			if (bgfx::isValid(handle)) bgfx::destroy(handle);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientVertexBuffer*)getPtr("trans");
			if (transBuffer) delete transBuffer;
		}
	}

	object& indexBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", bgfx::kInvalidHandle},
		};
		return proto;
	}

	indexBuffer::indexBuffer() : obj() {}

	indexBuffer::indexBuffer(object config) : obj(config) {
		setParent(getPrototype());
		uint16_t flags = 0;
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto handle = bgfx::createIndexBuffer(mem, flags);
			setUInt16("idx", handle.idx);
		} else if (type == dynamicBufferType) {
			auto bin = getStringView("data");
			auto mem =
				bgfx::makeRef(bin.data(), uint32_t(bin.size()));
			auto handle = bgfx::createDynamicIndexBuffer(mem, flags);
			setUInt16("idx", handle.idx);
		} else if (type == transientBufferType) {
			auto transBuffer = new bgfx::TransientIndexBuffer();
			auto count = getUInt16("count");
			bgfx::allocTransientIndexBuffer(transBuffer, count);
			setPtr("trans", transBuffer);
		}
	}

	void indexBuffer::set() {
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto handle = bgfx::IndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setIndexBuffer(handle);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicIndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setIndexBuffer(handle);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientIndexBuffer*)getPtr("trans");
			bgfx::setIndexBuffer(transBuffer);
		}
	}

	void indexBuffer::set(uint32_t start, uint32_t num) {
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto handle = bgfx::IndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setIndexBuffer(handle, start, num);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicIndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			bgfx::setIndexBuffer(handle, start, num);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientIndexBuffer*)getPtr("trans");
			bgfx::setIndexBuffer(transBuffer, start, num);
		}
	}

	void indexBuffer::destroy() {
		auto type = getUInt8("type");
		if (type == standardBufferType) {
			auto handle = bgfx::IndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			if (bgfx::isValid(handle)) bgfx::destroy(handle);
		} else if (type == dynamicBufferType) {
			auto handle = bgfx::DynamicIndexBufferHandle{
				getUInt16("idx", bgfx::kInvalidHandle)};
			if (bgfx::isValid(handle)) bgfx::destroy(handle);
		} else if (type == transientBufferType) {
			auto transBuffer =
				(bgfx::TransientIndexBuffer*)getPtr("trans");
			if (transBuffer) delete transBuffer;
		}
	}

}  // namespace gold
