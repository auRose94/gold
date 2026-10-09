#include "graphics.hpp"
#include "goldjs.hpp"
#include "renderStateBits.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <file.hpp>
#include <filesystem>
#include <fstream>
#include <game/renderBackend.hpp>
#include <game/windowSystem.hpp>
#include <image.hpp>
#include <iostream>
#include <set>
#include <sstream>

namespace gold {
	using namespace std;

	/**
	 * Expand a varying definition's #if/#ifdef/#ifndef/#else/#endif
	 * lines against `defines` (the ';'-separated NAME=value list the
	 * compile passes to the tool). Directive-free output satisfies every
	 * shaderc vintage; the system tool rejects directives in varying
	 * definitions outright. Returns the expanded temp file's path, ""
	 * when it couldn't be written (the caller then omits the varying and
	 * the tool's own error surfaces).
	 */
	string expandVaryingDefinition(const string& varyingPath,
		const string& defines) {
		ifstream in(varyingPath);
		if (!in) return varyingPath;

		set<string> definedSet;
		stringstream defineStream(defines);
		string part;
		while (getline(defineStream, part, ';')) {
			if (part.empty()) continue;
			auto cut = part.find('=');
			if (cut != string::npos) part = part.substr(0, cut);
			definedSet.insert(part);
		}

		// Condition grammar: or -> and ('||' and)*; and -> unary
		// ('&&' unary)*; unary -> '!' unary | '(' or ')' | defined( id )
		// | id (a bare macro names itself).
		struct condCursor {
			const string& text;
			size_t i = 0;
			const set<string>& defined;

			void skipSpace() {
				while (i < text.size() && text[i] == ' ') ++i;
			}

			bool parseOr() {
				auto out = parseAnd();
				for (;;) {
					skipSpace();
					if (i + 1 < text.size() && text[i] == '|' &&
						text[i + 1] == '|') {
						i += 2;
						out = out || parseAnd();
						continue;
					}
					return out;
				}
			}

			bool parseAnd() {
				auto out = parseUnary();
				for (;;) {
					skipSpace();
					if (i + 1 < text.size() && text[i] == '&' &&
						text[i + 1] == '&') {
						i += 2;
						out = out && parseUnary();
						continue;
					}
					return out;
				}
			}

			bool parseUnary() {
				skipSpace();
				if (i < text.size() && text[i] == '!') {
					++i;
					return !parseUnary();
				}
				return parseAtom();
			}

			bool parseAtom() {
				skipSpace();
				if (i >= text.size()) return false;
				if (text[i] == '(') {
					++i;
					auto out = parseOr();
					skipSpace();
					if (i < text.size() && text[i] == ')') ++i;
					return out;
				}
				const string token = "defined";
				if (text.compare(i, token.size(), token) == 0) {
					i += token.size();
					skipSpace();
					if (i < text.size() && text[i] == '(') ++i;
					skipSpace();
					const string id = ident();
					skipSpace();
					if (i < text.size() && text[i] == ')') ++i;
					return defined.count(id) > 0;
				}
				return defined.count(ident()) > 0;
			}

			string ident() {
				skipSpace();
				string id;
				while (i < text.size() &&
					   (isalnum((unsigned char)text[i]) ||
						   text[i] == '_'))
					id += text[i++];
				return id;
			}
		};

		string expanded;
		// Each frame is one open conditional chain (true = content
		// flows).
		vector<char> stack{1};
		auto active = [&stack]() {
			for (auto s : stack)
				if (!s) return false;
			return true;
		};

		string line;
		while (getline(in, line)) {
			size_t t = 0;
			while (t < line.size() &&
				   (line[t] == ' ' || line[t] == '\t'))
				++t;
			string text = line.substr(t);
			if (!text.empty() && text[0] == '#') {
				size_t d = 1;
				while (d < text.size() && text[d] == ' ') ++d;
				size_t start = d;
				while (d < text.size() &&
					   (isalnum((unsigned char)text[d]) ||
						   text[d] == '_'))
					++d;
				const string directive =
					text.substr(start, d - start);
				if (directive == "endif") {
					if (stack.size() > 1) stack.pop_back();
					continue;
				}
				if (directive == "else") {
					if (stack.size() > 1) stack.back() = !stack.back();
					continue;
				}
				if (directive == "if" || directive == "ifdef" ||
					directive == "ifndef") {
					condCursor cur{text, d, definedSet};
					bool out;
					if (directive == "ifdef") {
						const string name = cur.ident();
						out = !name.empty() && definedSet.count(name) > 0;
					} else if (directive == "ifndef") {
						const string name = cur.ident();
						out = name.empty() || definedSet.count(name) == 0;
					} else
						out = cur.parseOr();
					stack.push_back(out ? 1 : 0);
					continue;
				}
				// Any other directive: drop the line.
				continue;
			}
			if (active()) expanded += line + "\n";
		}

		const string tempPath =
			filesystem::temp_directory_path().string() +
			"/gold-varying-" +
			to_string(hash<string>()(varyingPath + "|" + defines)) +
			".def.sc";
		ofstream out(tempPath);
		if (!out) return "";
		out << expanded;
		return tempPath;
	}

	renderBackend*& gfxBackend::render() {
		static renderBackend* backend = nullptr;
		return backend;
	}

	renderBackend* gfxBackend::backend() { return render(); }

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
			{"screenshot", method(&gfxBackend::screenshot)},
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

	var gfxBackend::screenshot(list args) {
		auto backend = gfxBackend::render();
		if (!backend || !backend->isValid())
			return genericError("No active render backend");
		const auto path = args.size() > 0 ? args[0].getString()
										  : string("screenshot");
		// An invalid handle means the main window's back buffer.
		backend->requestScreenShot(renderHandle{}, path.c_str());
		return var(this);
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

		// Create the render backend through the registry, by the config
		// name itself ("bgfx"/"sdlgpu"; "sdl" aliases "sdlgpu"). A name
		// with no built-in factory (e.g. "vulkan") tries plugin::load
		// first and falls back to bgfx when nothing provides it.
		auto rb = getString("renderBackend", "bgfx");
		if (rb == "sdl") rb = "sdlgpu";
		auto backend = createRenderBackend(rb);
		if (!backend) backend = createRenderBackend(renderBackendType::BGFX);
		if (!backend) return genericError("No render backend available");
		render() = backend;

		auto cfg = obj({
			// The selection string gold-side; backends parse their own
			// rendering-API names from it (and accept the numeric form).
			{"rendererType", getVar("backend")},
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

		if (auto backend = render()) {
			for (auto it = shaderProgram::uniforms.begin();
				 it != shaderProgram::uniforms.end(); ++it) {
				auto o = it->second;
				auto uniform = renderHandle{
					o.getUInt16("idx", uint16_t(0xFFFF))};
				backend->destroyUniform(uniform);
			}
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
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	frameBuffer::frameBuffer() : obj() {}
	frameBuffer::frameBuffer(object config) : obj(config) {
		setParent(getPrototype());
		// The config object IS the descriptor; the backend interprets the
		// same shapes it always did.
		auto backend = gfxBackend::backend();
		auto handle = backend && backend->isValid()
						  ? backend->createFrameBuffer(config)
						  : renderHandle{};
		if (handle.valid()) setUInt16("idx", handle.idx);
	}

	// (The backend's descriptor create reads the config object directly.)

	void frameBuffer::setName(string name) {
		if (auto backend = gfxBackend::backend())
			backend->setObjectName(
				renderHandle{getUInt16("idx", uint16_t(0xFFFF))},
				name.c_str());
	}

	var frameBuffer::getTexture(uint8_t attachment) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (!backend || !handle.valid()) return var();
		auto texHandle = backend->getTexture(handle, attachment);
		if (!texHandle.valid()) return var();
		return gpuTexture(obj{{"idx", texHandle.idx}});
	}

	void frameBuffer::setViewFrameBuffer(uint16_t viewId) {
		if (auto backend = gfxBackend::backend())
			backend->setViewFrameBuffer(
				(uint8_t)viewId,
				renderHandle{getUInt16("idx", uint16_t(0xFFFF))});
	}

	void frameBuffer::requestScreenShot(string path) {
		if (auto backend = gfxBackend::backend())
			backend->requestScreenShot(
				renderHandle{getUInt16("idx", uint16_t(0xFFFF))},
				path.c_str());
	}

	void frameBuffer::destroy() {
		// The facade's destroy owns nothing itself: the descriptor's
		// destroyTextures flag told the backend how to treat attachments.
		empty();
	}

	object& occlusionQuery::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	occlusionQuery::occlusionQuery() : obj() {}
	occlusionQuery::occlusionQuery(object config) : obj(config) {
		setParent(getPrototype());
		if (auto backend = gfxBackend::backend()) {
			auto handle = backend->createOcclusionQuery();
			setUInt16("idx", handle.idx);
		}
	}

	occlusionQuery::queryResult occlusionQuery::getResult(
		int32_t* result) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (backend && handle.valid())
			return backend->getQueryResult(handle, result);
		return occlusionQuery::queryResult::NoResult;
	}

	void occlusionQuery::setCondition(bool visible) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			if (handle.valid()) backend->setCondition(handle, visible);
	}

	void occlusionQuery::destroy() {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->destroyQuery(handle);
		empty();
	}

	object& indirectBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	indirectBuffer::indirectBuffer() : obj() {}

	indirectBuffer::indirectBuffer(object config) : obj(config) {
		setParent(getPrototype());
	}

	void indirectBuffer::destroy() {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->destroyIndirect(handle);
		empty();
	}

	object& shaderObject::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
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
			auto view = gfxBackend::backend();
			auto bin = v.getStringView();
			auto strData =
				string_view(bin.data(), bin.size());
			auto h = std::hash<string_view>();
			setString("hash", to_string((uint64_t)h(strData)));
			if (view) {
				auto handle = view->createShader(
					bin.data(), uint32_t(bin.size()));
				setUInt16("idx", handle.idx);
			}
		} else if (s.isString()) {
			// Compile from source
			auto type = char(getUInt8("type", uint8_t('c')));
			auto path = filesystem::path(s.getString());
			// The shader library's include dir: the ORIGINAL source's
			// directory (the .sh include library) — captured before the
			// io-generation may re-point me at the temp compile file.
			const auto libraryDir =
				filesystem::path(path).parent_path().string();
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
				auto now =
					to_string(duration_cast<std::chrono::milliseconds>(
											std::chrono::high_resolution_clock::now()
												.time_since_epoch())
											.count());
				// The generated source compiles from a temp dir beside an
				// EXPANDED (directive-free) varying.def.sc: the tool's own
				// sibling lookup supplies the declarations, and varying
				// conditionals are resolved per-compile (some shaderc
				// vintages reject directives in varying definitions).
				string tempDir =
					filesystem::temp_directory_path().string();
				string shaderTempDir = tempDir + "/shaders/" + now;
				string tempPath = shaderTempDir + "/" +
					(path.filename().empty()
						 ? string("main.sc")
						 : path.filename().string());
				filesystem::create_directories(shaderTempDir);
				// The expanded varying rides the same dir.
				const auto expandedVarying = expandVaryingDefinition(
					varying, defines.empty() ? string() : defines);
				if (!expandedVarying.empty() &&
					expandedVarying != varying) {
					auto varyingBytes =
						file::readFile(expandedVarying).getObject<file>();
					auto data = varyingBytes.getBinary("data");
					auto vout = ofstream(
						shaderTempDir + "/varying.def.sc",
						ofstream::binary);
					vout.write(
						reinterpret_cast<const char*>(data.data()),
						(std::streamsize)data.size());
				}
				// Keep passing it explicitly too; the sibling is a
				// fallback for tool vintages that ignore the flag.
				varying = expandedVarying.empty() ? varying
												  : expandedVarying;
				auto tmpf = fopen(tempPath.c_str(), "w");
				if (!tmpf) {
					empty();
					setString("error", "Could not create temporary shader file");
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
			// The backend owns its compiler: it shells its toolchain and
			// hands back a created stage (an invalid handle from a backend
			// with no source-compile path). The include dirs: the original
			// source's directory (the shader's own .sh library) and the
			// varying's directory.
			if (auto backend = gfxBackend::backend()) {
				auto handle = backend->compileStage(jo(
					"type", type == 'v' ? "vertex"
						: type == 'f' ? "fragment" : "compute",
					"path", path.string(),
					"defines", defines,
					"varying", varying,
					"includeDirs",
						ja(libraryDir,
							filesystem::path(varying)
								.parent_path()
								.string())));
				if (handle.valid()) {
					setUInt16("idx", handle.idx);
				} else {
					cerr << "Failed to build: " << path << endl;
					empty();
					setString(
						"error", "Failed to compile shader: " + path.string());
				}
			}
		}
		if (getType("name") == typeString)
			cache[getString("name")] = *this;
		else
			cache[to_string((uint64_t)this)] = *this;
	}

	var shaderObject::getAllUniforms(list) {
		if (auto backend = gfxBackend::backend()) {
			auto handle = renderHandle{
				getUInt16("idx", uint16_t(0xFFFF))};
			auto uniformNames = vector<string>();
			backend->setShaderUniforms(handle, uniformNames);
			// No callers today; the shape carries the uniform names, one
			// empty entry each.
			auto uniformData = obj{};
			for (const auto& name : uniformNames)
				uniformData.setString(name, "");
			return uniformData;
		}
		return var();
	}

	void shaderObject::destroy() {
		if (auto backend = gfxBackend::backend())
			backend->destroyShader(
				renderHandle{getUInt16("idx", uint16_t(0xFFFF))});
		empty();
	}

	object& shaderProgram::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
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
				if (auto backend = gfxBackend::backend())
					setUInt16("idx",
						backend->createProgram(
							renderHandle{vert.getUInt16("idx",
								uint16_t(0xFFFF))},
							renderHandle{frag.getUInt16("idx",
								uint16_t(0xFFFF))})
							.idx);
			}
		} else if (getType("comp") == typeObject) {
			auto comp = getObject<shaderObject>("comp");
			if (comp) {
				if (auto backend = gfxBackend::backend())
					setUInt16("idx",
						backend->createProgram(
							renderHandle{
								comp.getUInt16("idx", uint16_t(0xFFFF))},
							renderHandle{})
							.idx);
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
		auto backend = gfxBackend::backend();
		if (backend) {
			auto handle = backend->createUniform(
				name.c_str(), t, num);
			if (handle.valid())
				uniforms[name] = obj{
					{"idx", handle.idx},
					{"type", (uint8_t)t},
					{"num", num},
				};
		}

		return true;
	}

	bool shaderProgram::setUniform(
		string name, const void* value, uint16_t num) {
		for (auto it = uniforms.begin(); it != uniforms.end();
				 ++it) {
			auto uniName = it->first;
			auto uniform = it->second;
			if (name.compare(uniName) == 0) {
				if (auto backend = gfxBackend::backend())
					backend->setUniform(
						renderHandle{uniform.getUInt16(
							"idx", uint16_t(0xFFFF))},
						value, num);
				return true;
			}
		}
		return false;
	}

	void shaderProgram::bindTexture(
		string sampler, uint8_t stage, gpuTexture tex) {
		if (!tex || !gfxBackend::backend()) return;
		auto uniform = uniforms[sampler];
		auto backend = gfxBackend::backend();
		if (!backend || !uniform) return;
		auto uniformHandle = renderHandle{
			uniform.getUInt16("idx", uint16_t(0xFFFF))};
		auto texHandle = renderHandle{
			tex.getUInt16("idx", uint16_t(0xFFFF))};
		if (!uniformHandle.valid() || !texHandle.valid())
			return;
		// 0 leaves the sampler flags as they were set at creation.
		backend->setTextureUniform(stage, uniformHandle, texHandle, 0);
	}

	inline void toLower(string& str) {
		std::transform(
			str.begin(), str.end(), str.begin(),
			[](unsigned char c) { return std::tolower(c); });
	}

	inline bool exists(string& str, string needle) {
		return str.find(needle) != string::npos;
	}

	/** One blend factor's gold field value from the vocabulary; 0 =
	 *  zero for anything unparseable. */
	inline uint64_t strToBlend(string value) {
		if (exists(value, "inv_src_color"))
			return uint64_t(stateBlendFactor::FactorInvSrcColor);
		else if (exists(value, "src_color"))
			return uint64_t(stateBlendFactor::FactorSrcColor);
		else if (exists(value, "inv_src_alpha"))
			return uint64_t(stateBlendFactor::FactorInvSrcAlpha);
		else if (exists(value, "dst_alpha"))
			return uint64_t(stateBlendFactor::FactorDstAlpha);
		else if (exists(value, "inv_dst_alpha"))
			return uint64_t(stateBlendFactor::FactorInvDstAlpha);
		else if (exists(value, "src_alpha_sat"))
			return uint64_t(stateBlendFactor::FactorSrcAlphaSat);
		else if (exists(value, "inv_dst_color"))
			return uint64_t(stateBlendFactor::FactorInvDstColor);
		else if (exists(value, "dst_color"))
			return uint64_t(stateBlendFactor::FactorDstColor);
		else if (exists(value, "inv_factor"))
			return uint64_t(stateBlendFactor::FactorInvBlendFactor);
		else if (exists(value, "factor"))
			return uint64_t(stateBlendFactor::FactorBlendFactor);
		else if (exists(value, "src_alpha"))
			return uint64_t(stateBlendFactor::FactorSrcAlpha);
		else if (exists(value, "one"))
			return uint64_t(stateBlendFactor::FactorOne);
		else if (exists(value, "zero"))
			return uint64_t(stateBlendFactor::FactorZero);
		return uint64_t(stateBlendFactor::FactorZero);
	}

	void shaderProgram::setState(object state) {
		uint64_t flags = 0;
		if (state.getType("flags") == typeNull) {
			auto write = state.getString("write");
			if (write != "") {
				toLower(write);
				if (exists(write, "r")) flags |= WriteR;
				if (exists(write, "g")) flags |= WriteG;
				if (exists(write, "b")) flags |= WriteB;
				if (exists(write, "a")) flags |= WriteA;
				if (exists(write, "z")) flags |= WriteZ;
			} else
				flags |= WriteR | WriteG | WriteB | WriteA | WriteZ;

			auto depth = state.getString("depth");
			if (depth != "") {
				toLower(depth);
				if (exists(depth, "less"))
					flags |= DepthLess << DepthCompareShift;
				else if (exists(depth, "lequal"))
					flags |= DepthLEqual << DepthCompareShift;
				else if (exists(depth, "equal"))
					flags |= DepthEqual << DepthCompareShift;
				else if (exists(depth, "gequal"))
					flags |= DepthGEqual << DepthCompareShift;
				else if (exists(depth, "greater"))
					flags |= DepthGreater << DepthCompareShift;
				else if (exists(depth, "notequal"))
					flags |= DepthNotEqual << DepthCompareShift;
				else if (exists(depth, "never"))
					flags |= DepthNever << DepthCompareShift;
				else if (exists(depth, "always"))
					flags |= DepthAlways << DepthCompareShift;
			} else {
				flags |= DepthLess << DepthCompareShift;
			}

			auto blend = state.getString("blend");
			auto blendA = state.getString("blendSrc");
			auto blendB = state.getString("blendDst");
			auto blendRGBSrc = state.getString("blendRGBSrc");
			auto blendRGBDst = state.getString("blendRGBDst");
			if (blend != "") {
				toLower(blend);
				// Equation keywords arm the blend and set the equation;
				// "independent"/"alpha_to_coverage" set their own bits.
				if (exists(blend, "independent"))
					flags |= BlendIndependent;
				else if (exists(blend, "alpha_to_coverage"))
					flags |= BlendAlphaToCoverage;
				else if (exists(blend, "add"))
					flags |= BlendEnabled |
							 (BlendAdd << BlendEquationShift);
				else if (exists(blend, "sub"))
					flags |= BlendEnabled |
							 (BlendSub << BlendEquationShift);
				else if (exists(blend, "revsub"))
					flags |= BlendEnabled |
							 (BlendRevSub << BlendEquationShift);
				else if (exists(blend, "min"))
					flags |= BlendEnabled |
							 (BlendMin << BlendEquationShift);
				else if (exists(blend, "max"))
					flags |= BlendEnabled |
							 (BlendMax << BlendEquationShift);
			} else if (
				blendRGBSrc != "" && blendRGBDst != "" &&
				blendA != "" && blendB != "") {
				uint64_t a = strToBlend(blendRGBSrc);
				uint64_t b = strToBlend(blendRGBDst);
				uint64_t x = strToBlend(blendA);
				uint64_t y = strToBlend(blendB);
				flags |= BlendEnabled |
						 ((a << BlendRGBSrcShift) | (b << BlendRGBDstShift) |
						  (x << BlendASrcShift) | (y << BlendADstShift));
			} else if (blendA != "" && blendB != "") {
				uint64_t a = strToBlend(blendA);
				uint64_t b = strToBlend(blendB);
				flags |= BlendEnabled |
						 ((a << BlendRGBSrcShift) | (b << BlendRGBDstShift) |
						  (a << BlendASrcShift) | (b << BlendADstShift));
			} else {
				// The old "normal" blend: src-alpha/inv-src-alpha, add.
				flags |= BlendEnabled |
						 (BlendAdd << BlendEquationShift) |
						 (uint64_t(stateBlendFactor::FactorSrcAlpha)
							 << BlendRGBSrcShift) |
						 (uint64_t(stateBlendFactor::FactorInvSrcAlpha)
							 << BlendRGBDstShift) |
						 (uint64_t(stateBlendFactor::FactorSrcAlpha)
							 << BlendASrcShift) |
						 (uint64_t(stateBlendFactor::FactorInvSrcAlpha)
							 << BlendADstShift);
			}

			auto cull = state.getString("cull");
			if (cull != "") {
				toLower(cull);
				// "ccw" contains "cw" as a substring — check the longer
				// token first, or "ccw" never reaches its own branch.
				if (exists(cull, "ccw"))
					flags |= CullCCW << CullShift;
				else if (exists(cull, "cw"))
					flags |= CullCW << CullShift;
			} else {
				flags |= CullCW << CullShift;
			}

			if (state.getVar("alphaRef").isNumber())
				// The field sits past bit 32: cast the source to the
				// state's 64-bit width FIRST or the 33-bit shift happens
				// in 32-bit and truncates (alphaRef silently zeroed).
				flags |= uint64_t(state.getUInt32("alphaRef") &
						 uint32_t(0xFF))
					<< AlphaRefShift;

			auto primitiveType = state.getString("type");
			if (primitiveType != "") {
				toLower(primitiveType);
				if (exists(primitiveType, "tristrip"))
					flags |= PrimitiveTriStrip << PrimitiveShift;
				else if (exists(primitiveType, "linestrip"))
					flags |= PrimitiveLineStrip << PrimitiveShift;
				else if (exists(primitiveType, "lines"))
					flags |= PrimitiveLines << PrimitiveShift;
				else if (exists(primitiveType, "points"))
					flags |= PrimitivePoints << PrimitiveShift;
				// "triangles"/anything else: no field = triangle list.
				// (The default used to be TRISTRIP, which read ordinary
				// glTF index buffers as strips and drew garbage.)
			}

			if (state.getType("MSAA") == typeBool && state.getBool("MSAA"))
				flags |= StateMSAA;

			if (state.getType("lineAA") == typeBool && state.getBool("lineAA"))
				flags |= StateLineAA;

			if (state.getType("conservative") == typeBool &&
				state.getBool("conservative"))
				flags |= StateConservativeRaster;
		} else {
			flags = state.getUInt64("flags");
		}
		auto color = state.getUInt32("blendColor");
		setUInt64("state", flags);
		setUInt32("blendColor", color);
		if (auto backend = gfxBackend::backend())
			backend->setState(flags, color);
	}

	void shaderProgram::defaultState() {
		// The parser's defaults (RGBA+Z writes, less, the normal blend,
		// cull cw) plus the old default's MSAA.
		setState(obj({{"write", ""}, {"MSAA", true}}));
	}

	void shaderProgram::setTransform(var& mtx) {
		if (auto backend = gfxBackend::backend())
			backend->setTransform(mtx.getPtr());
	}

	uint32_t parseStencil(
		string test, string fS, string fZ, string pZ) {
		uint32_t flags = 0;
		toLower(test);
		toLower(fS);
		toLower(fZ);
		toLower(pZ);
		auto compare = [](string t) -> uint32_t {
			if (exists(t, "less")) return uint32_t(DepthLess);
			if (exists(t, "lequal")) return uint32_t(DepthLEqual);
			if (exists(t, "equal")) return uint32_t(DepthEqual);
			if (exists(t, "gequal")) return uint32_t(DepthGEqual);
			if (exists(t, "greater")) return uint32_t(DepthGreater);
			if (exists(t, "notequal")) return uint32_t(DepthNotEqual);
			if (exists(t, "never")) return uint32_t(DepthNever);
			if (exists(t, "always")) return uint32_t(DepthAlways);
			return uint32_t(DepthAlways);
		};
		auto op = [](string t) -> uint32_t {
			if (exists(t, "zero")) return uint32_t(OpZero);
			if (exists(t, "replace")) return uint32_t(OpReplace);
			if (exists(t, "incrsat")) return uint32_t(OpIncrSat);
			if (exists(t, "incr")) return uint32_t(OpIncr);
			if (exists(t, "decrsat")) return uint32_t(OpDecrSat);
			if (exists(t, "decr")) return uint32_t(OpDecr);
			if (exists(t, "invert")) return uint32_t(OpInvert);
			if (exists(t, "keep")) return uint32_t(OpKeep);
			return uint32_t(OpKeep);
		};
		flags |= compare(test) << StencilTestShift;
		flags |= op(fS) << StencilFailShift;
		flags |= op(fZ) << StencilZFailShift;
		flags |= op(pZ) << StencilZPassShift;
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

		if (auto backend = gfxBackend::backend())
			backend->setStencil(fstencil, bstencil);
	}

	void shaderProgram::defaultStencil() {
		// No stencil: test always, every op keep — the gold layout's zero
		// state happens to mean exactly that.
		if (auto backend = gfxBackend::backend())
			backend->setStencil(0, 0);
	}

	void shaderProgram::setDiscard(string state) {
		toLower(state);
		uint16_t flag = 0;
		if (exists(state, "all"))
			flag |= DiscardAll;
		else {
			if (exists(state, "bindings"))
				flag |= DiscardBindings;
			if (exists(state, "index_buffer"))
				flag |= DiscardIndexBuffer;
			if (exists(state, "instance_data"))
				flag |= DiscardInstanceData;
			if (exists(state, "state"))
				flag |= DiscardState;
			if (exists(state, "transform"))
				flag |= DiscardTransform;
			if (exists(state, "vertex_streams"))
				flag |= DiscardVertexStreams;
		}
		setUInt16("discard", flag);
	}
	void shaderProgram::defaultDiscard() { erase("discard"); }

	void shaderProgram::submit(uint8_t viewId, uint32_t depth) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		uint16_t flags = getUInt16("discard", uint16_t(DiscardAll));
		if (auto backend = gfxBackend::backend())
			backend->submit(viewId, handle, depth, flags);
	}

	void shaderProgram::submit(
		uint8_t viewId, occlusionQuery query, uint32_t depth) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		auto qHandle = renderHandle{
			query.getUInt16("idx", uint16_t(0xFFFF))};
		uint16_t flags = getUInt16("discard", uint16_t(DiscardAll));
		if (auto backend = gfxBackend::backend())
			backend->submitQuery(viewId, handle, qHandle, depth, flags);
	}
	void shaderProgram::submit(
		uint8_t viewId, indirectBuffer buffer, uint16_t start,
		uint16_t num, uint32_t depth) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		auto iHandle = renderHandle{
			buffer.getUInt16("idx", uint16_t(0xFFFF))};
		uint16_t flags = getUInt16("discard", uint16_t(DiscardAll));
		if (auto backend = gfxBackend::backend())
			backend->submitIndirect(viewId, handle, iHandle, start, num,
				depth, flags);
	}

	void shaderProgram::dispatch(
		uint8_t viewId, uint32_t numX, uint32_t numY,
		uint32_t numZ) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		uint16_t flags = getUInt16("discard", uint16_t(DiscardAll));
		if (auto backend = gfxBackend::backend())
			backend->dispatch(viewId, handle, numX, numY, numZ, flags);
	}

	void shaderProgram::dispatch(
		uint8_t viewId, indirectBuffer buffer, uint16_t start,
		uint16_t num) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		auto iHandle = renderHandle{
			buffer.getUInt16("idx", uint16_t(0xFFFF))};
		uint16_t flags = getUInt16("discard", uint16_t(DiscardAll));
		if (auto backend = gfxBackend::backend())
			backend->dispatchIndirect(viewId, handle, iHandle, start, num,
				flags);
	}

	void shaderProgram::destroy() {
		if (auto backend = gfxBackend::backend())
			backend->destroyProgram(
				renderHandle{getUInt16("idx", uint16_t(0xFFFF))});
		empty();
	}

	object& gpuTexture::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	uint64_t sampleStringToFlags(string value) {
		uint32_t flags = 0;

		if (exists(value, "u_mirror"))
			flags |= MirrorU;
		else if (exists(value, "u_clamp"))
			flags |= ClampU;
		else if (exists(value, "u_border"))
			flags |= BorderU;

		if (exists(value, "v_mirror"))
			flags |= MirrorV;
		else if (exists(value, "v_clamp"))
			flags |= ClampV;
		else if (exists(value, "v_border"))
			flags |= BorderV;

		if (exists(value, "w_mirror"))
			flags |= MirrorW;
		else if (exists(value, "w_clamp"))
			flags |= ClampW;
		else if (exists(value, "w_border"))
			flags |= BorderW;

		if (exists(value, "min_point"))
			flags |= MinPoint;
		else if (exists(value, "min_anis"))
			flags |= MinAnisotropic;

		if (exists(value, "mag_point"))
			flags |= MagPoint;
		else if (exists(value, "mag_anis"))
			flags |= MagAnisotropic;

		if (exists(value, "mip_point"))
			flags |= MipPoint;

		if (exists(value, "less"))
			flags |= CompareEnabled |
					 (uint32_t(DepthLess) << CompareModeShift);
		else if (exists(value, "lequal"))
			flags |= CompareEnabled |
					 (uint32_t(DepthLEqual) << CompareModeShift);
		else if (exists(value, "gequal"))
			flags |= CompareEnabled |
					 (uint32_t(DepthGEqual) << CompareModeShift);
		else if (exists(value, "equal"))
			flags |= CompareEnabled |
					 (uint32_t(DepthEqual) << CompareModeShift);
		else if (exists(value, "greater"))
			flags |= CompareEnabled |
					 (uint32_t(DepthGreater) << CompareModeShift);
		else if (exists(value, "notequal"))
			flags |= CompareEnabled |
					 (uint32_t(DepthNotEqual) << CompareModeShift);
		else if (exists(value, "never"))
			flags |= CompareEnabled |
					 (uint32_t(DepthNever) << CompareModeShift);
		else if (exists(value, "always"))
			flags |= CompareEnabled |
					 (uint32_t(DepthAlways) << CompareModeShift);

		return flags;
	}

	/** The image-container path: an image facade decodes the binary
	 *  (PNG/DDS/KTX2 via the system bimg-backed codecs) and its parsed
	 *  fields replace mine; the texture then creates from them. */
	renderHandle gpuTexture::parseData(binary& bin) {
		auto flagsStr = getString("flags");
		toLower(flagsStr);
		uint64_t flags = sampleStringToFlags(flagsStr);
		auto handle = renderHandle{uint16_t(0xFFFF)};
		auto img = image({{"data", bin}});
		copy(img);
		auto pData = getStringView("data");

		auto backend = gfxBackend::backend();
		if (!backend) return handle;
		const auto format = (texFormat)getUInt32("format");
		if (getUInt32("depth") > 0) {
			auto width = getUInt16("width");
			auto height = getUInt16("height");
			auto depth = getUInt16("depth");
			auto hasMips = getUInt8("numMips") > 0;
			handle = backend->createTexture3D(
				width, height, depth, hasMips, format, flags, pData.data(),
				uint32_t(pData.size()));
		} else if (getBool("cubeMap")) {
			auto size = getUInt16("width");
			auto numLayers = getUInt16("numLayers");
			auto hasMips = getUInt8("numMips") > 0;
			handle = backend->createTextureCube(
				size, hasMips, numLayers, format, flags, pData.data(),
				uint32_t(pData.size()));
		} else {
			auto width = getUInt16("width");
			auto height = getUInt16("height");
			auto numLayers = getUInt16("numLayers");
			auto hasMips = getUInt8("numMips") > 0;
			if (width != 0 && height != 0)
				handle = backend->createTexture2D(
					width, height, hasMips, numLayers, format, flags,
					pData.data(), uint32_t(pData.size()));
		}
		return handle;
	}

	gpuTexture::gpuTexture() : obj() { setParent(getPrototype()); }
	gpuTexture::gpuTexture(object config) : obj(config) {
		setParent(getPrototype());
		auto flagsStr = getString("flags");
		toLower(flagsStr);
		uint64_t flags = sampleStringToFlags(flagsStr);
		auto name = getString("name");
		if (name == "") name = getString("path");
		if (name == "") name = to_string((uint64_t)this);

		auto backend = gfxBackend::backend();
		renderHandle handle;
		auto sizeVar = config.getVar("size");
		auto depthVar = config.getVar("depth");
		auto widthVar = config.getVar("width");
		auto heightVar = config.getVar("height");
		auto binData = getVar("data");
		std::string_view bytes;
		if (backend) {
			const auto format = (texFormat)getUInt32(
				"format", uint32_t(texFormat::Count));
			if (sizeVar && sizeVar.getUInt16() != 0) {
				bytes = binData.getStringView();
				auto layers = getUInt16("layers");
				handle = backend->createTextureCube(sizeVar.getUInt16(),
					getBool("hasMips"), layers, format, flags, bytes.data(),
					uint32_t(bytes.size()));
			} else if (
				depthVar && depthVar.getUInt16() != 0 && widthVar &&
				widthVar.getUInt16() != 0 && heightVar &&
				heightVar.getUInt16() != 0) {
				bytes = binData.getStringView();
				handle = backend->createTexture3D(
					widthVar.getUInt16(), heightVar.getUInt16(),
					depthVar.getUInt16(), getBool("hasMips"), format, flags,
					bytes.data(), uint32_t(bytes.size()));
			} else if (
				widthVar && widthVar.getUInt16() != 0 && heightVar &&
				heightVar.getUInt16() != 0) {
				bytes = binData.getStringView();
				handle = backend->createTexture2D(
					widthVar.getUInt16(), heightVar.getUInt16(),
					getBool("hasMips"), getUInt16("layers", uint16_t(0)),
					format, flags, bytes.data(),
					uint32_t(bytes.size()));
			} else if (binData.isView()) {
				auto b = binData.getBinary();
				handle = parseData(b);
			} else if (config.getType("path") == typeString) {
				auto path = getString("path");
				if (path.size() > 0) {
					// Load from file, set to object
					auto textRet =
						file::readFile(path).getObject<file>();
					auto fileData = textRet.getBinary("data");
					handle = parseData(fileData);
				}
			}
		}
		if (handle.valid()) {
			backend->setObjectName(handle, name.c_str());
			setUInt16("idx", handle.idx);
			cache[name] = *this;
		}
	}

	void gpuTexture::update(object info) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (!backend || !handle.valid()) return;
		auto mip = getUInt8("mip");
		auto sideVar = info.getVar("side");
		auto depthVar = info.getVar("depth");
		if (sideVar && sideVar.getUInt8() < 6) {
			auto bin = getStringView("data");
			backend->updateTexture(
				handle, (uint8_t)sideVar.getUInt8(), mip, bin.data(),
				uint32_t(bin.size()));
		} else if (depthVar && depthVar.getUInt16() != 0) {
			auto bin = getStringView("data");
			backend->updateTexture3D(handle, mip, bin.data(),
				uint32_t(bin.size()));
		} else {
			auto bin = getStringView("data");
			backend->updateTexture2D(handle, mip, bin.data(),
				uint32_t(bin.size()));
		}
	}

	void gpuTexture::setImage(
		uint8_t stage, uint8_t mip, accessType t, textureFormat f) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->setImage(stage, handle, mip, t, f);
	}

	void gpuTexture::blit(
		uint8_t viewId, var dstP, gpuTexture src, var srcP,
		var size) {
		auto dstHandle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto srcHandle = renderHandle{
			src.getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (!backend || !dstHandle.valid() || !srcHandle.valid()) return;
		uint16_t w = UINT16_MAX, h = UINT16_MAX;
		if (size.isVec2()) {
			w = size.getUInt16(0);
			h = size.getUInt16(1);
		}
		backend->blit(viewId, dstHandle, 0, dstP.getUInt16(0),
			dstP.getUInt16(1), 0, srcHandle, 0, srcP.getUInt16(0),
			srcP.getUInt16(1), 0, w, h, 1);
	}
	void gpuTexture::blit(
		uint8_t viewId, uint8_t dstMip, var dstP, gpuTexture src,
		uint8_t srcMip, var srcP, var size) {
		auto dstHandle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto srcHandle = renderHandle{
			src.getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (!backend || !dstHandle.valid() || !srcHandle.valid()) return;
		uint16_t w = UINT16_MAX, h = UINT16_MAX, d = UINT16_MAX;
		if (size.isVec3()) {
			w = size.getUInt16(0);
			h = size.getUInt16(1);
			d = size.getUInt16(2);
		}
		backend->blit(viewId, dstHandle, dstMip, dstP.getUInt16(0),
			dstP.getUInt16(1), dstP.getUInt16(2), srcHandle, srcMip,
			srcP.getUInt16(0), srcP.getUInt16(1), srcP.getUInt16(2), w, h,
			d);
	}

	uint32_t gpuTexture::readTexture(void* bin, uint8_t mip) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (backend && backend->readTexture(handle, bin, mip)) return 0;
		return UINT32_MAX;  // the caller treats non-zero as failed
	}
	void* gpuTexture::getDirectAccessPtr() {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		auto backend = gfxBackend::backend();
		if (!backend || !handle.valid()) return nullptr;
		return backend->directAccessPtr(handle);
	}
	void gpuTexture::setName(string name) {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->setObjectName(handle, name.c_str());
	}
	void gpuTexture::destroy() {
		auto handle = renderHandle{
			getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->destroyTexture(handle);
		setUInt16("idx", uint16_t(0xFFFF));
	}

	object& vertexLayout::getPrototype() {
		static auto proto = obj{
			{"descriptor", list()},
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

	// The layout is pure gold descriptor data now: begin() opens the
	// "descriptor" list, add() appends {attrib, count, type, normalized,
	// asInt} entries, and the backend materializes it at buffer time.
	vertexLayout& vertexLayout::begin() {
		setList("descriptor", list({}));
		return *this;
	}

	vertexLayout& vertexLayout::add(
		attrib att, attribType t, uint8_t count, bool norm,
		bool isInt) {
		auto descriptor = getList("descriptor");
		descriptor.pushObject(obj{
			{"attrib", (uint8_t)att},
			{"type", (uint8_t)t},
			{"count", count},
			{"normalized", norm},
			{"asInt", isInt},
		});
		setList("descriptor", descriptor);
		return *this;
	}

	vertexLayout& vertexLayout::end() { return *this; }

	void vertexLayout::destroy() { empty(); }

	object& vertexBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	vertexBuffer::vertexBuffer() : obj() {}
	vertexBuffer::vertexBuffer(object config) : obj(config) {
		setParent(getPrototype());
		auto type = getUInt8("type", nullBufferType);
		// The layout may be a bare (prototype-less) gold object: use the
		// untyped var access so descriptor data is all we need.
		auto layoutVar = getVar("layout");
		auto layoutObj =
			layoutVar.isObject() ? layoutVar.getObject() : object();
		auto backend = gfxBackend::backend();
		if (!backend) return;
		if (type == standardBufferType) {
			auto bytes = getStringView("data");
			auto handle = backend->createVertexBuffer(
				bytes.data(), uint32_t(bytes.size()), layoutObj, 0);
			setUInt16("idx", handle.idx);
		} else if (type == dynamicBufferType) {
			auto bytes = getStringView("data");
			auto handle = backend->createDynamicVertexBuffer(
				bytes.data(), uint32_t(bytes.size()), layoutObj, 0);
			setUInt16("idx", handle.idx);
		} else if (type == transientBufferType) {
			auto count = getUInt16("count");
			if (count >= 1) {
				auto handle =
					backend->createTransientVertexBuffer(layoutObj, count);
				setUInt16("idx", handle.idx);
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
			// The backend keeps the transient's backing data.
			if (auto backend = gfxBackend::backend()) {
				auto handle = renderHandle{
					getUInt16("idx", uint16_t(0xFFFF))};
				backend->updateVertexBuffer(handle, bin.data(),
					uint32_t(end - start), uint32_t(start), 0);
			}
		}
	}

	void vertexBuffer::set(uint8_t stream) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->setVertexBuffer(stream, handle, 0, 0);
	}
	void vertexBuffer::set(
		uint8_t stream, uint32_t start, uint32_t num, vertexLayout) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			// The transient's layout lives in the backend's allocation;
			// static/dynamic buffers carry theirs from creation.
			backend->setVertexBuffer(stream, handle, start, num);
	}

	void vertexBuffer::destroy() {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->destroyBuffer(handle);
	}

	object& indexBuffer::getPrototype() {
		static auto proto = obj{
			{"idx", uint16_t(0xFFFF)},
		};
		return proto;
	}

	indexBuffer::indexBuffer() : obj() {}

	indexBuffer::indexBuffer(object config) : obj(config) {
		setParent(getPrototype());
		auto backend = gfxBackend::backend();
		auto type = getUInt8("type");
		if (!backend) return;
		auto bytes = getStringView("data");
		// The width the data was packed at (glTF UNSIGNED_INT vs
		// UNSIGNED_SHORT); the backend reads it as gold's BufferIndex32.
		const uint64_t flags = getBool("index32", false)
			? uint64_t(BufferIndex32) : uint64_t(BufferNone);
		if (type == standardBufferType) {
			auto handle = backend->createIndexBuffer(
				bytes.data(), uint32_t(bytes.size()), flags);
			setUInt16("idx", handle.idx);
		} else if (type == dynamicBufferType) {
			auto handle = backend->createDynamicIndexBuffer(
				bytes.data(), uint32_t(bytes.size()), flags);
			setUInt16("idx", handle.idx);
		} else if (type == transientBufferType) {
			auto handle = backend->createTransientIndexBuffer(
				getUInt16("count"), flags);
			setUInt16("idx", handle.idx);
		}
	}

	void indexBuffer::set() {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->setIndexBuffer(handle, 0, 0);
	}

	void indexBuffer::set(uint32_t start, uint32_t num) {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->setIndexBuffer(handle, start, num);
	}

	void indexBuffer::destroy() {
		auto handle = renderHandle{getUInt16("idx", uint16_t(0xFFFF))};
		if (auto backend = gfxBackend::backend())
			backend->destroyBuffer(handle);
	}

}  // namespace gold
