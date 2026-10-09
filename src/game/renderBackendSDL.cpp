#include "game/renderBackend.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "game/graphics.hpp"
#include "game/renderStateBits.hpp"

#ifndef GOLD_SHADER_COMPILER
#define GOLD_SHADER_COMPILER "bgfx-shaderc"
#endif
#ifndef GOLD_SDL_GLSLANG_COMPILER
#define GOLD_SDL_GLSLANG_COMPILER "glslangValidator"
#endif

namespace gold {

	using namespace std;

	namespace {

		// SDL3 GPU (SDL_GPU) render backend.
		//
		// SDL_GPU on Linux is the Vulkan driver; its SPIR-V convention is
		// one uniform block per stage (set 1 for the vertex stage, set 3
		// for fragment, std140) and combined image samplers in set 0 /
		// set 2. Two paths produce stages:
		//
		//  - createShader receives the embedded bgfx blobs (sprite and
		//    wireframe): a uniform/sampler table, then bgfx's binary-
		//    wrapped SPIR-V. glslang auto-bound everything to set 0, so
		//    the DescriptorSet/Binding literal decorations are rewritten
		//    in place before SDL_GPUShader creation.
		//  - compileStage shells the bgfx preprocessor (the GLSL text the
		//    GL path compiles today), rewrites it into Vulkan GLSL
		//    (explicit io locations, one std140 uniform block, the bgfx
		//    NDC helpers as constants) and shells glslangValidator for the
		//    SPIR-V. The converter builds the uniform table itself, so
		//    both paths converge on the same stageRecord shape.
		//
		// The io locations are the gold vertexAttrib ids on every stage
		// (vertex inputs and the v_* varyings), which are also what the
		// vertex input descriptors use — stages link by construction. The
		// blob path reflects the SPIR-V's own assignments per stage.
		//
		// Frame lifecycle: beginFrame opens the submission window (and
		// recycles the previous frame's transient buffers, bgfx's ring
		// semantics); calls accrue (uniform shadows, pending uploads, draw
		// state); endFrame runs the copy pass for the uploads, walks the
		// views (ascending view id, bgfx sort), presents, and retires the
		// transfer buffers.

		// --- gold resource records ----------------------------------------
		struct uniformRecord {
			string name;
			renderUniformType type = renderUniformType::Vec4;
			uint16_t num = 1;
			vector<uint8_t> shadow;  // vec4 16*num, mat3 36*num, mat4 64*num
		};

		// One uniform-block member: the byte offset the SPIR-V assigns it
		// and the vec4-slot count (bgfx's regCount form; mat4 = 4).
		struct stageUniform {
			string name;
			renderUniformType type = renderUniformType::Vec4;
			uint16_t num = 1;
			uint32_t offset = 0;
			uint16_t slots = 1;
		};

		struct stageSampler {
			string name;
		};

		struct stageRecord {
			SDL_GPUShader* shader = nullptr;
			bool fragment = false;
			uint32_t blockSize = 0;
			vector<stageUniform> uniforms;
			vector<stageSampler> samplers;
			// The SPIR-V's vertex-input locations ("a_*" name -> location).
			// Blob-path shaders assign their own; compileStage assigns the
			// gold ids through the converter. The pipeline's vertex
			// descriptors map gold attribs through this table, mirroring
			// bgfx's per-program glGetAttribLocation.
			map<string, uint32_t> ioLocations;
		};

		struct programRecord {
			uint16_t vs = 0xFFFF;
			uint16_t fs = 0xFFFF;
		};

		// A gold layout descriptor materialized for SDL_GPU: the slot-0
		// stride and the attribute list in descriptor order.
		struct layoutMaterial {
			uint32_t stride = 0;
			struct attrib {
				uint8_t id;  // the gold vertexAttrib enum id
				uint32_t offset;
				SDL_GPUVertexElementFormat format;
			};
			vector<attrib> attribs;
		};

		struct bufferRecord {
			SDL_GPUBuffer* buffer = nullptr;
			bool isIndex = false;
			bool isDynamic = false;
			bool isTransient = false;
			bool index32 = false;
			uint32_t size = 0;
			layoutMaterial layout;
			// Transients stage through CPU memory; the frame-end copy pass
			// uploads what was touched.
			vector<uint8_t> transientData;
			bool transientDirty = false;
		};

		struct textureRecord {
			SDL_GPUTexture* texture = nullptr;
			SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
			SDL_GPUTextureType type = SDL_GPU_TEXTURETYPE_2D;
			uint32_t width = 0, height = 0, depth = 1;
			uint8_t numMips = 1;
			uint16_t numLayers = 1;
			bool cube = false;
			SDL_GPUSampler* sampler = nullptr;  // cached per texture
			uint32_t samplerFlags = 0;
		};

		// --- format maps ----------------------------------------------------
		// gold's texFormat enum mirrors bgfx's ordering for the cast; the
		// SDL map is explicit so drift shows as INVALID instead of wrong
		// bytes. Compressed formats land here when their block math is
		// wired.
		SDL_GPUTextureFormat toSDLFormat(texFormat f, bool srgb) {
			switch (f) {
			case texFormat::RGBA8:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			case texFormat::BGRA8:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
			case texFormat::R8:
				return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
			case texFormat::A8:
				return SDL_GPU_TEXTUREFORMAT_A8_UNORM;
			case texFormat::RG8:
				return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
			case texFormat::R16F:
				return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
			case texFormat::RGBA16F:
				return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
			case texFormat::R32F:
				return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
			case texFormat::RGBA32F:
				return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
			case texFormat::BC1:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
			case texFormat::BC3:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
			case texFormat::BC7:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
			default:
				return SDL_GPU_TEXTUREFORMAT_INVALID;
			}
		}

		// Byte size of one mip block in the dense layout. Only the
		// uncompressed 4-byte/px formats (plus their smaller cousins) are
		// mapped; anything else falls back to 4 bytes/px.
		uint32_t toMipBytes(SDL_GPUTextureFormat format, uint32_t w,
			uint32_t h, uint32_t d) {
			switch (format) {
			case SDL_GPU_TEXTUREFORMAT_A8_UNORM:
			case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
			case SDL_GPU_TEXTUREFORMAT_R8_SNORM:
				return w * h * d;
			case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
			case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
			case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
			case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
			case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
				return 2u * w * h * d;
			case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
				return 16u * w * h * d;
			default:
				// R8G8B8A8*/B8G8R8A8*/RGBA16F/R32F and friends today.
				return 4u * w * h * d;
			}
		}

		// --- toolchain -----------------------------------------------------
		// Fork/exec: stdout is the payload, stderr the messages. Both pipes
		// drain before the waitpid so a chatty tool cannot wedge.
		binary runTool(vector<string> args, string* stderrText) {
			vector<char*> argv;
			argv.reserve(args.size() + 1);
			for (auto& a : args) argv.push_back(a.data());
			argv.push_back(nullptr);

			int outFds[2], errFds[2];
			if (pipe(outFds) != 0) return binary();
			if (pipe(errFds) != 0) {
				close(outFds[0]);
				close(outFds[1]);
				return binary();
			}
			pid_t pid = fork();
			if (pid == 0) {
				dup2(outFds[1], STDOUT_FILENO);
				dup2(errFds[1], STDERR_FILENO);
				close(outFds[0]);
				close(outFds[1]);
				close(errFds[0]);
				close(errFds[1]);
				execvp(args[0].c_str(), argv.data());
				_exit(127);
			}
			close(outFds[1]);
			close(errFds[1]);
			vector<uint8_t> out;
			string errText;
			char buf[65536];
			ssize_t n;
			while ((n = read(outFds[0], buf, sizeof(buf))) > 0)
				out.insert(out.end(), buf, buf + n);
			close(outFds[0]);
			while ((n = read(errFds[0], buf, sizeof(buf))) > 0)
				errText.append(buf, buf + n);
			close(errFds[0]);
			int status = 0;
			waitpid(pid, &status, 0);
			if (stderrText) *stderrText = errText;
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0
				|| out.empty()) {
				// Surface the tool's own exit for the missing-output case
				// (empty stderr + empty stdout explains nothing by
				// itself).
				fprintf(stderr,
					"[sdlgpu] tool %s did not produce output (exit %d, "
					"stderr %zu bytes)\n",
					args.size() > 0 ? args[0].c_str() : "?",
					WIFEXITED(status) ? WEXITSTATUS(status) : -1,
					errText.size());
				return binary();
			}
			return binary(out.begin(), out.end());
		}

		// Shell glslangValidator: GLSL text in, SPIR-V out.
		binary glslangToSpirv(const string& glsl, SDL_GPUShaderStage stage,
			string* errorText) {
			const string tempDir =
				filesystem::temp_directory_path().string();
			const string tempPath = tempDir + "/gold-sdl-gpu-" +
				to_string(hash<string>()(
					glsl + to_string((int)stage))) +
				(stage == SDL_GPU_SHADERSTAGE_VERTEX ? ".vert" : ".frag");
			{
				ofstream out(tempPath, ofstream::binary);
				if (!out) {
					if (errorText)
						*errorText =
							"could not write the shader temp file";
					return binary();
				}
				out << glsl;
			}
			const vector<string> args {
				GOLD_SDL_GLSLANG_COMPILER,
				"-V",
				"-S",
				stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vert" : "frag",
				"-o", tempPath + ".spv",
				tempPath,
			};
			string stderrText;
			auto bin = runTool(args, &stderrText);
			// The compiled module rides the -o file; stdout is empty even
			// on success. Read it back; on empty, report the tool's
			// stderr (the compile errors).
			const string modulePath = tempPath + ".spv";
			{
				ifstream spv(modulePath, ifstream::binary);
				if (spv) {
					auto size = spv.seekg(0, ifstream::end).tellg();
					if (size > 20) {
						spv.seekg(0);
						vector<uint8_t> bytes(
							size_t(size), uint8_t(0));
						spv.read((char*)bytes.data(), bytes.size());
						return binary(bytes.begin(), bytes.end());
					}
				}
			}
			if (errorText && !stderrText.empty())
				*errorText = stderrText;
			return binary();
		}

		// --- bgfx blob parse -----------------------------------------------
		bool isBGFXShaderBlob(const void* data, uint32_t size) {
			if (!data || size < 8) return false;
			const auto bytes = (const uint8_t*)data;
			const bool vsh = bytes[0] == 'V' && bytes[1] == 'S';
			const bool fsh = bytes[0] == 'F' && bytes[1] == 'S';
			const bool csh = bytes[0] == 'C' && bytes[1] == 'S';
			return (vsh || fsh || csh) && bytes[2] == 'H';
		}

		// The wrapper: magic, input/output hashes, two RawBindings mask
		// words, a uniform/sampler table, then the SPIR-V. regIndex is the
		// member's BYTE offset in its UniformBlock; regCount counts vec4
		// slots (mat4 = 4, mat3 = 3). Fragment-stage entries carry bgfx's
		// 0x10 bit.
		bool parseBGFXBlob(const void* data, uint32_t size, stageRecord& out,
			binary& spirv) {
			if (!isBGFXShaderBlob(data, size)) return false;
			const auto bytes = (const uint8_t*)data;
			const uint32_t magic = bytes[0] | (bytes[1] << 8) |
				(bytes[2] << 16) | (uint32_t(bytes[3]) << 24);
			const uint8_t version = uint8_t((magic >> 24) & 0xFF);
			out.fragment = bytes[0] == 'F';
			uint32_t off = 4;      // magic
			off += 8;              // inputHash + outputHash
			off += 8;              // RawBindings srv/uav masks
			const uint16_t count =
				uint16_t(bytes[off] | (bytes[off + 1] << 8));
			off += 2;
			out.uniforms.clear();
			out.samplers.clear();
			out.ioLocations.clear();
			for (uint16_t i = 0; i < count; ++i) {
				const uint8_t nameSize = bytes[off++];
				if (off + nameSize > size) return false;
				string name((const char*)bytes + off, nameSize);
				off += nameSize;
				uint8_t typeCode = bytes[off++];
				uint8_t num = bytes[off++];
				uint32_t regIndex =
					uint32_t(bytes[off] | (bytes[off + 1] << 8));
				off += 2;
				uint16_t regCount =
					uint16_t(bytes[off] | (bytes[off + 1] << 8));
				off += 2;
				uint8_t texComponent = 0, texDimension = 0;
				uint16_t texFormat = 0;
				if (version >= 8) {
					texComponent = bytes[off];
					texDimension = bytes[off + 1];
					off += 2;
				}
				if (version >= 10) {
					texFormat =
						uint16_t(bytes[off] | (bytes[off + 1] << 8));
					off += 2;
				}
				(void)texComponent;
				(void)texDimension;
				(void)texFormat;

				typeCode &= ~0xF8;  // ref|fragment|sampler|readonly|compare bits
				// bgfx types: Sampler=0, End=1, Vec4=2, Mat3=3, Mat4=4.
				// Samplers join the bind list; End entries (storage
				// buffers and anything else) are skipped.
				if (typeCode > 4) continue;
				if (typeCode == 0) {
					out.samplers.push_back({name});
					continue;
				}
				if (typeCode == 1) continue;

				stageUniform entry;
				entry.name = name;
				entry.num = num;
				entry.offset = regIndex;
				entry.slots = regCount;
				switch (typeCode) {
				case 2:
					entry.type = renderUniformType::Vec4;
					break;
				case 3:
					entry.type = renderUniformType::Mat3;
					break;
				default:  // 4
					entry.type = renderUniformType::Mat4;
					break;
				}
				out.uniforms.push_back(entry);
				out.blockSize = max(out.blockSize,
					regIndex + uint32_t(regCount) * 16);
			}

			const uint32_t codeSize =
				bytes[off] | (bytes[off + 1] << 8) |
				(bytes[off + 2] << 16) | (uint32_t(bytes[off + 3]) << 24);
			off += 4;
			if (off + codeSize > size) return false;
			spirv.assign(bytes + off, bytes + off + codeSize);
			return true;
		}

		// --- SPIR-V utilities ----------------------------------------------
		// An instruction walker over the module's words. Only the header
		// opcodes the backend reads exist here: OpName (id -> string) and
		// OpDecorate (Location values for inputs).
		struct spirvWalker {
			const uint32_t* words = nullptr;
			uint32_t wordCount = 0;

			bool begin(const void* data, uint32_t size) {
				if (size < 20 || (size & 3)) return false;
				words = (const uint32_t*)data;
				if (words[0] != 0x07230203) return false;
				wordCount = size / 4;
				return true;
			}

			void forAll(std::function<bool(uint32_t op,
				const uint32_t* operands, uint32_t operandCount)> visit)
				const {
				for (uint32_t i = 5; i < wordCount;) {
					const uint32_t op = words[i] & 0xFFFF;
					const uint32_t wc = (words[i] >> 16) & 0xFFFF;
					if (wc == 0) return;  // malformed; stop reading
					// Zero-operand instructions (OpReturn, OpFunctionEnd)
					// are legal: skip them, nothing to visit.
					if (wc < 2) { i += wc; continue; }
					if (i + wc > wordCount) return;
					if (!visit(op, words + i + 1, wc - 1)) return;
					i += wc;
				}
			}

			// The declared vertex inputs with their Location decorations.
			map<string, uint32_t> collectInputLocations() const {
				map<uint32_t, string> idNames;
				map<uint32_t, uint32_t> locations;
				forAll([&](uint32_t op, const uint32_t* ops,
						   uint32_t count) {
					if (op == 5 /* OpName */ && count >= 2) {
						const char* str = (const char*)&ops[1];
						idNames[ops[0]] = str;
					} else if (op == 71 /* OpDecorate */ && count >= 3) {
						if (ops[1] == 30 /* Location */)
							locations[ops[0]] = ops[2];
					}
					return true;
				});
				map<string, uint32_t> result;
				for (auto& [id, name] : idNames) {
					if (name.rfind("a_", 0) != 0) continue;
					auto loc = locations.find(id);
					if (loc != locations.end())
						result[name] = loc->second;
				}
				return result;
			}
		};

		// The blob rewrite: glslang auto-bound bgfx's SPIR-V everything to
		// set 0; SDL wants uniform buffers in set 1 (vertex) / set 3
		// (fragment) and combined samplers in set 0 / set 2, bound in the
		// blob table's order (the same order the draw binds them in). Only
		// literal decoration words change, so the patch runs in place.
		bool rewriteSPIRVBindings(binary& spirv, const stageRecord& stage) {
			const size_t totalWords = spirv.size() / 4;
			if (totalWords < 5) return false;
			const auto words = (uint32_t*)spirv.data();
			if (words[0] != 0x07230203) return false;

			spirvWalker walk;
			walk.begin(spirv.data(), uint32_t(spirv.size()));
			map<uint32_t, uint32_t> ptrClass;  // pointer type -> class
			map<uint32_t, string> idNames;
			walk.forAll([&](uint32_t op, const uint32_t* ops,
							uint32_t count) {
				if (op == 5 && count >= 2)
					idNames[ops[0]] = string((const char*)&ops[1]);
				else if (op == 32 && count >= 3)  // OpTypePointer
					ptrClass[ops[0]] = ops[1];
				return true;
			});
			map<uint32_t, uint32_t> varClass;  // variable id -> class
			walk.forAll([&](uint32_t op, const uint32_t* ops,
							uint32_t count) {
				// OpVariable: [resultType, resultId, storageClass...]
				if (op == 59 && count >= 3) {
					const auto ptrIt = ptrClass.find(ops[0]);
					if (ptrIt != ptrClass.end())
						varClass[ops[1]] = ptrIt->second;
				}
				return true;
			});

			map<string, uint32_t> samplerSlot;
			for (uint32_t i = 0; i < stage.samplers.size(); ++i)
				samplerSlot[stage.samplers[i].name] = i;

			uint32_t uboCount = 0;
			bool ok = true;
			for (uint32_t i = 5; i < totalWords && ok;) {
				const uint32_t op = words[i] & 0xFFFF;
				const uint32_t wc = (words[i] >> 16) & 0xFFFF;
				if (wc == 0) { ok = false; break; }
				if (wc < 2) { i += wc; continue; }
				if (i + wc > totalWords) break;
				if (op == 71 /* OpDecorate */ && wc >= 4) {
					// operands: result id, decoration, literal...
					const uint32_t id = words[i + 1];
					const uint32_t deco = words[i + 2];
					const auto varIt = varClass.find(id);
					if (varIt != varClass.end()) {
						const uint32_t cls = varIt->second;
						if (deco == 34 /* DescriptorSet */) {
							if (cls == 2)  // Uniform: the one block
								words[i + 3] = stage.fragment ? 3u : 1u;
							else if (cls == 3)  // UniformConstant
								words[i + 3] = stage.fragment ? 2u : 0u;
						} else if (deco == 33 /* Binding */) {
							if (cls == 2)
								words[i + 3] = uboCount++;
							else if (cls == 3) {
								const string name =
									idNames.count(id) ? idNames[id] : "";
								const auto slot = samplerSlot.find(name);
								if (slot != samplerSlot.end())
									words[i + 3] = slot->second;
							}
						}
					}
				}
				i += wc;
			}
			if (!ok) fprintf(stderr,
				"[sdlgpu] rewrite: stream walk failed\n");
			else if (uboCount > 4) fprintf(stderr,
				"[sdlgpu] rewrite: %u uniform blocks\n", uboCount);
			return ok && uboCount <= 4;
		}

		bool validateSPIRVMagic(const binary& spirv) {
			return spirv.size() >= 20 && spirv.size() % 4 == 0
				&& spirv[0] == 0x03 && spirv[1] == 0x02
				&& spirv[2] == 0x23 && spirv[3] == 0x07;
		}

		// --- GLSL conversion (compileStage path) ---------------------------
		// The io locations are the gold ids: vertex inputs on the vertex
		// stage, and the v_* varyings on both stages at the same ids so
		// the io links.
		uint32_t semanticLocation(const string& goldName) {
			auto stem = goldName.size() > 2 && (goldName[0] == 'a'
												   || goldName[0] == 'v')
					&& goldName[1] == '_'
				? goldName.substr(2) : goldName;
			if (stem == "position") return uint32_t(vertexAttrib::Position);
			if (stem == "normal") return uint32_t(vertexAttrib::Normal);
			if (stem == "tangent") return uint32_t(vertexAttrib::Tangent);
			if (stem == "bitangent")
				return uint32_t(vertexAttrib::Bitangent);
			if (stem == "color0") return uint32_t(vertexAttrib::Color0);
			if (stem == "color1") return uint32_t(vertexAttrib::Color1);
			if (stem == "color2") return uint32_t(vertexAttrib::Color2);
			if (stem == "color3") return uint32_t(vertexAttrib::Color3);
			if (stem == "indices") return uint32_t(vertexAttrib::Indices);
			if (stem == "weight") return uint32_t(vertexAttrib::Weight);
			// texcoord0..7: the TexCoord0 slot is 10.
			if (stem.rfind("texcoord", 0) == 0 && stem.size() > 9)
				return uint32_t(vertexAttrib::TexCoord0) +
					uint32_t(stem[9] - '0');
			return 0;
		}

		uint32_t std140UniformSize(renderUniformType type, uint16_t count) {
			switch (type) {
			case renderUniformType::Mat4: return 64u * count;
			case renderUniformType::Mat3: return 48u * count;
			default: return 16u * count;
			}
		}

		// Whole-word (identifier) match.
		bool identifierUsed(const string& body, const string& name) {
			size_t pos = 0;
			while ((pos = body.find(name, pos)) != string::npos) {
				const bool left = pos == 0 ||
					!(isalnum((unsigned char)body[pos - 1]) ||
						body[pos - 1] == '_');
				const size_t after = pos + name.size();
				const bool right = after >= body.size() ||
					!(isalnum((unsigned char)body[after]) ||
						body[after] == '_');
				if (left && right) return true;
				pos = after;
			}
			return false;
		}

		/** Rewrite the pp's GLSL text into Vulkan GLSL; fill the stage
		 *  record's uniform table (std140 offsets in block declaration
		 *  order) and the sampler order. Unused sampler declarations drop
		 *  (SDL caps them at 16 per stage). */
		string convertGLSLToVulkan(const string& glsl,
			SDL_GPUShaderStage stage, stageRecord& record) {
			const bool fragment = stage == SDL_GPU_SHADERSTAGE_FRAGMENT;
			const uint32_t uniformSet = fragment ? 3u : 1u;
			const uint32_t samplerSet = fragment ? 2u : 0u;
			record.fragment = fragment;

			// Split into lines.
			vector<string> lines;
			{
				size_t pos = 0;
				while (pos <= glsl.size()) {
					auto nl = glsl.find('\n', pos);
					if (nl == string::npos) {
						if (pos < glsl.size())
							lines.push_back(glsl.substr(pos));
						break;
					}
					lines.push_back(glsl.substr(pos, nl - pos));
					pos = nl + 1;
				}
			}

			vector<string> keep;
			size_t blockAt = string::npos;
			vector<stageUniform> entries;
			vector<stageSampler> samplers;
			uint32_t blockOffset = 0;

			for (auto& line : lines) {
				const auto lead = line.find_first_not_of(" \t");
				auto st = lead == string::npos
					? "" : line.substr(lead);
				auto indent = lead == string::npos
					? string() : line.substr(0, lead);

				if (st.rfind("uniform ", 0) == 0) {
					// uniform TYPE name[n];
					const size_t typeStart = 8;
					const auto typeEnd =
						st.find_first_of(" \t", typeStart);
					if (typeEnd == string::npos) {
						keep.push_back(line);
						continue;
					}
					auto glslType =
						st.substr(typeStart, typeEnd - typeStart);
					auto name = st.substr(
						typeEnd + 1, st.rfind(';') - (typeEnd + 1));
					string plainName = name;
					uint32_t count = 1;
					const auto bracket = name.find('[');
					if (bracket != string::npos) {
						plainName = name.substr(0, bracket);
						const auto close = name.find(']', bracket);
						count = uint32_t(max(1ll, atoll(
							name.substr(bracket + 1,
								close - bracket - 1).c_str())));
					}
					const bool samplerKind =
						glslType.rfind("sampler", 0) == 0
							|| glslType.rfind("image", 0) == 0;
					if (samplerKind) {
						// Keep it only when the shader reads it (SDL caps
						// stage samplers at 16; unused declarations would
						// burn slots). The sweep excludes this
						// declaration's own line.
						string bodyless;
						for (auto& other : lines) {
							if (other == line) continue;
							bodyless += other + "\n";
						}
						if (!identifierUsed(bodyless, plainName))
							continue;
						samplers.push_back({plainName});
						keep.push_back(indent + "layout(set = " +
							to_string(samplerSet) + ", binding = " +
							to_string(samplers.size() - 1) + ") " + st);
						continue;
					}
					if (plainName == "bgfx_ndc") {
						// bgfx's NDC patch table for a top-left,
						// homogeneous-depth renderer (Vulkan).
						keep.push_back(indent +
							"const vec4 bgfx_ndc = "
							"vec4(2.0, 1.0, -1.0, 0.5);");
						continue;
					}
					if (plainName == "bgfx_indirectArgBase") {
						keep.push_back(indent +
							"const vec4 bgfx_indirectArgBase = "
							"vec4(0.0, 0.0, 0.0, 0.0);");
						continue;
					}
					// A plain uniform joins the std140 block.
					const auto type = glslType == "mat4"
							? renderUniformType::Mat4
						: glslType == "mat3"
							? renderUniformType::Mat3
							: renderUniformType::Vec4;
					uint16_t slots = 1;
					switch (type) {
					case renderUniformType::Mat4:
						slots = uint16_t(4 * count);
						break;
					case renderUniformType::Mat3:
						slots = uint16_t(3 * count);
						break;
					default:
						slots = uint16_t(count);
						break;
					}
					stageUniform entry;
					entry.name = plainName;
					entry.type = type;
					entry.num = uint16_t(count);
					entry.offset = blockOffset;
					entry.slots = slots;
					blockOffset += std140UniformSize(
						type, uint16_t(count));
					entries.push_back(entry);
					// The block takes the first member's place (decls
					// must precede use).
					if (blockAt == string::npos)
						blockAt = keep.size();
					continue;
				}
				if (st.rfind("attribute ", 0) == 0
					|| st.rfind("varying ", 0) == 0) {
					const bool isAttrib = st.rfind("attribute ", 0) == 0;
					const size_t head = isAttrib ? 10 : 8;
					const auto typeEnd = st.find_first_of(" \t", head);
					if (typeEnd == string::npos) {
						keep.push_back(line);
						continue;
					}
					auto glslType = st.substr(head, typeEnd - head);
					auto name = st.substr(
						typeEnd + 1, st.rfind(';') - (typeEnd + 1));
					const auto loc = semanticLocation(name);
					keep.push_back(indent + "layout(location = " +
						to_string(loc) + ") " +
						(isAttrib
							? "in "
							: (fragment ? "in " : "out ")) +
						glslType + " " + name + ";");
					continue;
				}
				keep.push_back(line);
			}

			if (blockAt != string::npos) {
				vector<string> block;
				block.push_back("layout(std140, set = " +
					to_string(uniformSet) + ", binding = 0) uniform "
					"UniformBlock {");
				for (auto& entry : entries) {
					const char* type =
						entry.type == renderUniformType::Mat4
							? "mat4"
						: entry.type == renderUniformType::Mat3
							? "mat3" : "vec4";
					block.push_back("    " + string(type) + " " +
						entry.name +
						(entry.num > 1
							? "[" + to_string(entry.num) + "]"
							: "") + ";");
				}
				block.push_back("};");
				keep.insert(keep.begin() + blockAt, block.begin(),
					block.end());
			} else {
				// No uniform block: SDL gets num_uniform_buffers = 0.
				blockOffset = 0;
			}

			record.uniforms = entries;
			record.samplers = samplers;
			record.blockSize = blockOffset;

			string body;
			for (auto& l : keep) body += l + "\n";
			if (fragment) {
				// The version first, then the explicit fragment output.
				string finalText =
					"#version 450\n"
					"layout(location = 0) out vec4 _gold_fragColor;\n" +
					body;
				size_t pos = 0;
				const string from = "gl_FragColor";
				while ((pos = finalText.find(from, pos)) != string::npos) {
					finalText.replace(pos, from.size(), "_gold_fragColor");
					pos += from.size();
				}
				return finalText;
			}
			return "#version 450\n" + body;
		}

		// The value size at set time (bgfx's g_uniformTypeSize contract:
		// mat3 items are 9 floats; the packet pads them into rows).
		uint32_t goldUniformValueSize(renderUniformType type, uint16_t num) {
			switch (type) {
			case renderUniformType::Mat4: return 64u * num;
			case renderUniformType::Mat3: return 36u * num;
			default: return 16u * num;
			}
		}

		// --- vertex materialization ----------------------------------------
		// SDL's vertex element formats: no 1-element byte/short kinds and
		// no packed 10-10-10 — such layouts map to INVALID here (the
		// attribute drops; the stride holds).
		SDL_GPUVertexElementFormat vertexElementFormat(
			vertexAttribType t, uint8_t count, bool normalized) {
			switch (t) {
			case vertexAttribType::Float:
				switch (count) {
				case 1: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
				case 2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
				case 3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
				case 4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
				}
				break;
			case vertexAttribType::Uint8:
				switch (count) {
				case 2:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2;
				case 4:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
				}
				break;
			case vertexAttribType::Int8:
				switch (count) {
				case 2:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_BYTE2;
				case 4:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_BYTE4;
				}
				break;
			case vertexAttribType::Int16:
				switch (count) {
				case 2:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_SHORT2;
				case 4:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_SHORT4;
				}
				break;
			case vertexAttribType::Uint16:
				switch (count) {
				case 2:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_USHORT2;
				case 4:
					return normalized
						? SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM
						: SDL_GPU_VERTEXELEMENTFORMAT_USHORT4;
				}
				break;
			case vertexAttribType::Half:
				switch (count) {
				case 2: return SDL_GPU_VERTEXELEMENTFORMAT_HALF2;
				case 4: return SDL_GPU_VERTEXELEMENTFORMAT_HALF4;
				}
				break;
			case vertexAttribType::Int32:
				switch (count) {
				case 1: return SDL_GPU_VERTEXELEMENTFORMAT_INT;
				case 2: return SDL_GPU_VERTEXELEMENTFORMAT_INT2;
				case 3: return SDL_GPU_VERTEXELEMENTFORMAT_INT3;
				case 4: return SDL_GPU_VERTEXELEMENTFORMAT_INT4;
				}
				break;
			case vertexAttribType::Uint32:
				switch (count) {
				case 1: return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
				case 2: return SDL_GPU_VERTEXELEMENTFORMAT_UINT2;
				case 3: return SDL_GPU_VERTEXELEMENTFORMAT_UINT3;
				case 4: return SDL_GPU_VERTEXELEMENTFORMAT_UINT4;
				}
				break;
			default:
				break;
			}
			return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
		}

		uint32_t vertexElementSize(vertexAttribType t, uint8_t count) {
			switch (t) {
			case vertexAttribType::Int8:
			case vertexAttribType::Uint8: return 1u * count;
			case vertexAttribType::Int16:
			case vertexAttribType::Uint16:
			case vertexAttribType::Half: return 2u * count;
			case vertexAttribType::Uint10: return 4u;
			default: return 4u * count;
			}
		}

		class sdlGpuBackend : public renderBackend {
		 SDL_GPUDevice* _device = nullptr;
		 SDL_Window* _window = nullptr;
		 SDL_GPUTextureFormat _format = SDL_GPU_TEXTUREFORMAT_INVALID;
		 uint32_t _clearColor = 0;
		 float _clearDepth = 1.0f;
		 uint8_t _clearStencil = 0;
		 uint16_t _width = 0, _height = 0;
		 uint16_t _viewX = 0, _viewY = 0;
		 uint16_t _viewW = 0, _viewH = 0;
		 uint16_t _clearFlags = ClearColor | ClearDepth;
		 bool _offscreen = false;

			// gold-side resource tables (uint16 handles, 1-based).
			uint16_t _nextUniform = 1, _nextStage = 1, _nextProgram = 1;
			uint16_t _nextBuffer = 1, _nextTexture = 1;
			map<uint16_t, uniformRecord> _uniforms;
			map<uint16_t, stageRecord> _stages;
			map<uint16_t, programRecord> _programs;
			map<uint16_t, bufferRecord> _buffers;
			map<uint16_t, textureRecord> _textures;

			struct pendingUpload {
				uint16_t buffer;
				SDL_GPUTransferBuffer* tb;
				uint32_t size;
				uint32_t offset;
			};
			struct pendingTex {
				uint16_t texture;
				SDL_GPUTransferBuffer* tb;
			};
			vector<pendingUpload> _pendingUploads;
			vector<pendingTex> _pendingTexUploads;

		 public:
			renderBackendType type() const override {
				return renderBackendType::SDLGPU;
			}
			const char* name() const override { return "sdlgpu"; }
			rendererKind kind() const override {
				// SDL_GPU's Linux driver is Vulkan (SPIR-V shaders).
				return rendererKind::Vulkan;
			}
			bool homogeneousDepth() const override { return true; }

			bool initialize(nativeWindow nw, object config) override {
				if (_device) return true;
				if (!SDL_WasInit(SDL_INIT_VIDEO)) {
					// A GPU device itself needs no window; the offscreen
					// video subsystem is enough (tests and CI use this).
					if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
						fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
						return false;
					}
					_offscreen = true;
				}
				_window = (SDL_Window*)nw.window;
				_device = SDL_CreateGPUDevice(
					SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
				if (!_device) {
					fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
					return false;
				}
				_width = config.getUInt16("width", 1360);
				_height = config.getUInt16("height", 800);
				_viewW = _width;
				_viewH = _height;
				_clearColor = config.getUInt32("rgba", 0x6ab0deff);
				_clearFlags = ClearColor | ClearDepth;
				if (_window) {
					if (!SDL_ClaimWindowForGPUDevice(_device, _window)) {
						fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
						SDL_DestroyGPUDevice(_device);
						_device = nullptr;
						return false;
					}
					_format =
						SDL_GetGPUSwapchainTextureFormat(_device, _window);
					_offscreen = false;
				}
				return true;
			}

			void destroy() override {
				if (_device) {
					// Release every recorded object; SDL defers the free
					// until the frame's command buffers stop using them.
					for (auto& [_, rec] : _buffers)
						SDL_ReleaseGPUBuffer(_device, rec.buffer);
					for (auto& [_, rec] : _textures) {
						if (rec.sampler)
							SDL_ReleaseGPUSampler(_device,
								rec.sampler);
						SDL_ReleaseGPUTexture(_device, rec.texture);
					}
					for (auto& [_, rec] : _stages)
						SDL_ReleaseGPUShader(_device, rec.shader);
					_uniforms.clear();
					_stages.clear();
					_programs.clear();
					_buffers.clear();
					_textures.clear();
					_pendingUploads.clear();
					_pendingTexUploads.clear();
					if (_window) {
						SDL_ReleaseWindowFromGPUDevice(_device, _window);
						_window = nullptr;
					}
					SDL_DestroyGPUDevice(_device);
					_device = nullptr;
				}
			}

			bool isValid() const override { return _device != nullptr; }

			// ---- views (kept as state; the pass walk is 4c) --------------
			void viewRect(uint8_t view, uint16_t x, uint16_t y, uint16_t w,
				uint16_t h) override {
				(void)view;
				_viewX = x;
				_viewY = y;
				_viewW = w ? w : _width;
				_viewH = h ? h : _height;
			}
			void viewClear(uint8_t view, uint16_t flags, uint32_t rgba,
				float depth, uint8_t stencil) override {
				(void)view;
				_clearFlags = flags;
				_clearColor = rgba;
				_clearDepth = depth;
				_clearStencil = stencil;
			}
			void viewTransform(uint8_t view, const void* viewMtx,
				const void* projMtx) override {
				(void)view;
				(void)viewMtx;
				(void)projMtx;
			}

			// ---- uniforms -------------------------------------------------
			renderHandle createUniform(const char* name, renderUniformType t,
				uint16_t num) override {
				for (auto& [idx, rec] : _uniforms)
					if (rec.name == name) return renderHandle{idx};
				const uint16_t idx = _nextUniform++;
				uniformRecord rec;
				rec.name = name;
				rec.type = t;
				rec.num = num;
				_uniforms[idx] = rec;
				return renderHandle{idx};
			}
			void setUniform(renderHandle h, const void* value, uint16_t num)
				override {
				auto it = _uniforms.find(h.idx);
				if (it == _uniforms.end() || !value) return;
				auto& rec = it->second;
				const uint32_t bytes = goldUniformValueSize(
					rec.type, uint16_t(min(num, rec.num)));
				rec.shadow.assign((const uint8_t*)value,
					(const uint8_t*)value + bytes);
			}
			void getUniformInfo(renderHandle h, string& name,
				renderUniformType& t) const override {
				auto it = _uniforms.find(h.idx);
				if (it == _uniforms.end()) {
					t = renderUniformType::Count;
					return;
				}
				name = it->second.name;
				t = it->second.type;
			}
			void setShaderUniforms(renderHandle h, vector<string>& out)
				override {
				auto it = _stages.find(h.idx);
				if (it == _stages.end()) return;
				for (auto& entry : it->second.uniforms)
					out.push_back(entry.name);
				for (auto& sampler : it->second.samplers)
					out.push_back(sampler.name);
			}
			void destroyUniform(renderHandle h) override {
				_uniforms.erase(h.idx);
			}

			// ---- shaders ---------------------------------------------------
			renderHandle createShader(const void* data, uint32_t size)
				override {
				// The embedded blobs arrive as bgfx's binary-wrapped
				// SPIR-V (sprite/wireframe).
				stageRecord rec;
				binary spirv;
				if (!parseBGFXBlob(data, size, rec, spirv)) {
					fprintf(stderr, "[sdlgpu] createShader: parse out (size %u)\n", size);
					return renderHandle{};
				}
				if (!validateSPIRVMagic(spirv)) {
					fprintf(stderr, "[sdlgpu] createShader: bad spirv magic\n");
					return renderHandle{};
				}
				// The blob's own io assignments are authoritative.
				spirvWalker walk;
				walk.begin(spirv.data(), uint32_t(spirv.size()));
				rec.ioLocations = walk.collectInputLocations();
				if (!rewriteSPIRVBindings(spirv, rec)) {
					fprintf(stderr, "[sdlgpu] createShader: rewrite out\n");
					return renderHandle{};
				}
				return createStageSDL(rec, spirv);
			}

			renderHandle createStageSDL(const stageRecord& rec,
				const binary& spirv) {
				SDL_GPUShaderCreateInfo ci {};
				ci.code_size = spirv.size();
				ci.code = (const uint8_t*)spirv.data();
				ci.entrypoint = "main";
				ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
				ci.stage = rec.fragment
					? SDL_GPU_SHADERSTAGE_FRAGMENT
					: SDL_GPU_SHADERSTAGE_VERTEX;
				// Combined image samplers for SPIR-V; SDL caps at 16 per
				// stage (the converter strips unused declarations).
				ci.num_samplers = uint32_t(rec.samplers.size());
				ci.num_uniform_buffers = rec.blockSize ? 1 : 0;
				SDL_GPUShader* shader = SDL_CreateGPUShader(_device, &ci);
				if (!shader) {
					fprintf(stderr, "[sdlgpu] shader create: %s\n",
						SDL_GetError());
					return renderHandle{};
				}
				const uint16_t idx = _nextStage++;
				_stages[idx] = rec;
				_stages[idx].shader = shader;
				return renderHandle{idx};
			}

			renderHandle compileStage(object request) override {
				auto type = request["type"].getString();
				const bool vertex = type == "vertex";
				const bool fragment = type == "fragment";
				if (!vertex && !fragment)
					return renderHandle{};  // compute arrives with 4c
				auto path = request["path"].getString();
				auto defines = request["defines"].getString();
				auto varyingPath = request["varying"].getString();
				auto includeDirs = vector<string>();
				for (const auto& dir :
					request.getList("includeDirs", list()))
					includeDirs.push_back(dir.getString());

				// Expand the varying first (this shaderc vintage rejects
				// directives inside it; the facade re-runs the same
				// expansion, so this usually re-reads the same temp file).
				const auto expanded = expandVaryingDefinition(
					varyingPath.empty() ? path : varyingPath,
					defines.empty() ? string() : defines);
				string activeVarying = varyingPath;
				if (!expanded.empty() && expanded != varyingPath)
					activeVarying = expanded;

				// Pass 1: the bgfx preprocessor produces the GLSL text
				// (what the GL path compiles today).
				vector<string> args {
					GOLD_SHADER_COMPILER, "-f", path,
					"--type", vertex ? "vertex" : "fragment",
					"--platform", "linux",
					"--profile", "330",
					"--preprocess", "--stdout",
				};
				if (!activeVarying.empty()
					&& filesystem::exists(
						filesystem::path(activeVarying))) {
					args.push_back("--varyingdef");
					args.push_back(activeVarying);
				}
				for (auto& dir : includeDirs)
					if (!dir.empty()) {
						args.push_back("-i");
						args.push_back(dir);
					}
				if (!defines.empty()) {
					args.push_back("--define");
					args.push_back(defines);
				}
				string stderrText;
				auto glslBin = runTool(args, &stderrText);
				if (glslBin.empty()) {
					cerr << stderrText << '\n';
					return renderHandle{};
				}
				string glslText(glslBin.begin(), glslBin.end());

				// Pass 2: rewrite into Vulkan GLSL, compile to SPIR-V.
				stageRecord rec;
				auto converted = convertGLSLToVulkan(
					glslText,
					vertex ? SDL_GPU_SHADERSTAGE_VERTEX
						   : SDL_GPU_SHADERSTAGE_FRAGMENT,
					rec);
				string errorText;
				auto spirv = glslangToSpirv(
					converted,
					vertex ? SDL_GPU_SHADERSTAGE_VERTEX
						   : SDL_GPU_SHADERSTAGE_FRAGMENT,
					&errorText);
				if (spirv.empty() || !validateSPIRVMagic(spirv)) {
					cerr << "[sdlgpu] compileStage " << path << ":\n"
						 << errorText << '\n';
					return renderHandle{};
				}
				// The SPIR-V's own assignment echoes the converter's; the
				// io map collects it for the pipeline descriptors.
				spirvWalker walk;
				walk.begin(spirv.data(), uint32_t(spirv.size()));
				rec.ioLocations = walk.collectInputLocations();
				return createStageSDL(rec, spirv);
			}

			renderHandle createProgram(renderHandle vs, renderHandle fs)
				override {
				const uint16_t idx = _nextProgram++;
				programRecord rec;
				rec.vs = vs.idx;
				rec.fs = fs.idx;
				_programs[idx] = rec;
				return renderHandle{idx};
			}
			void destroyProgram(renderHandle h) override {
				_programs.erase(h.idx);
			}
			void destroyShader(renderHandle h) override {
				auto it = _stages.find(h.idx);
				if (it == _stages.end()) return;
				SDL_ReleaseGPUShader(_device, it->second.shader);
				_stages.erase(it);
			}

			// ---- buffers ---------------------------------------------------
			layoutMaterial materializeLayout(object layoutObj) const {
				layoutMaterial lay;
				if (!layoutObj) return lay;
				auto entries = layoutObj.getList("descriptor");
				uint32_t offset = 0;
				for (auto& entry : entries) {
					auto e = entry.getObject();
					if (!e) continue;
					const auto id = e.getUInt8("attrib");
					const auto count = e.getUInt8("count");
					const auto vtype =
						(vertexAttribType)e.getUInt8("type");
					const bool normalized = e.getBool("normalized");
					// asInt folds into the non-normalized integer kinds;
					// SDL's formats express it that way.
					auto fmt = vertexElementFormat(vtype, count,
						normalized);
					if (fmt == SDL_GPU_VERTEXELEMENTFORMAT_INVALID)
						continue;
					lay.attribs.push_back({id, offset, fmt});
					offset += vertexElementSize(vtype, count);
				}
				lay.stride = offset;
				return lay;
			}

			renderHandle createBufferLike(const void* data, uint32_t size,
				bool isIndex, bool dynamic, uint64_t flags,
				const layoutMaterial& layout) {
				SDL_GPUBufferUsageFlags usage =
					isIndex ? SDL_GPU_BUFFERUSAGE_INDEX
							: SDL_GPU_BUFFERUSAGE_VERTEX;
				SDL_GPUBufferCreateInfo bi {};
				bi.usage = usage;
				bi.size = size ? size : 1;
				SDL_GPUBuffer* buf = SDL_CreateGPUBuffer(_device, &bi);
				if (!buf) {
					fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
					return renderHandle{};
				}
				const uint16_t idx = _nextBuffer++;
				bufferRecord rec;
				rec.buffer = buf;
				rec.isIndex = isIndex;
				rec.isDynamic = dynamic;
				rec.index32 = (flags & BufferIndex32) != 0;
				rec.size = bi.size;
				rec.layout = layout;
				_buffers[idx] = rec;
				if (data && size)
					pendingBufferUpload(idx, data, size, 0);
				return renderHandle{idx};
			}

			void pendingBufferUpload(uint16_t idx, const void* data,
				uint32_t size, uint32_t start) {
				SDL_GPUTransferBufferCreateInfo ti {};
				ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				ti.size = size ? size : 1;
				SDL_GPUTransferBuffer* tb =
					SDL_CreateGPUTransferBuffer(_device, &ti);
				if (!tb) return;
				auto* dst = SDL_MapGPUTransferBuffer(_device, tb, false);
				memcpy(dst, data, size);
				SDL_UnmapGPUTransferBuffer(_device, tb);
				_pendingUploads.push_back({idx, tb, size, start});
			}

			renderHandle createVertexBuffer(const void* data, uint32_t size,
				object layoutDesc, uint64_t flags) override {
				return createBufferLike(data, size, false, false, flags,
					materializeLayout(layoutDesc));
			}
			renderHandle createDynamicVertexBuffer(const void* data,
				uint32_t size, object layoutDesc, uint64_t flags) override {
				return createBufferLike(data, size, false, true, flags,
					materializeLayout(layoutDesc));
			}
			renderHandle createIndexBuffer(const void* data, uint32_t size,
				uint64_t flags) override {
				return createBufferLike(data, size, true, false, flags,
					layoutMaterial());
			}
			renderHandle createDynamicIndexBuffer(const void* data,
				uint32_t size, uint64_t flags) override {
				return createBufferLike(data, size, true, true, flags,
					layoutMaterial());
			}
			// Transients: bgfx's ring chunks — allocate the full slot,
			// stage writes in CPU memory, one upload at frame end.
			renderHandle createTransientVertexBuffer(object layoutDesc,
				uint16_t count) override {
				auto layout = materializeLayout(layoutDesc);
				const uint16_t idx = _nextBuffer++;
				SDL_GPUBufferCreateInfo bi {};
				bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
				bi.size = max<uint32_t>(1, layout.stride * count);
				SDL_GPUBuffer* buf = SDL_CreateGPUBuffer(_device, &bi);
				if (!buf) return renderHandle{};
				bufferRecord rec;
				rec.buffer = buf;
				rec.isTransient = true;
				rec.size = bi.size;
				rec.layout = layout;
				rec.transientData.assign(bi.size, uint8_t(0));
				_buffers[idx] = rec;
				return renderHandle{idx};
			}
			renderHandle createTransientIndexBuffer(uint16_t count,
				uint64_t flags) override {
				const uint16_t idx = _nextBuffer++;
				const uint32_t bytes =
					count * ((flags & BufferIndex32) ? 4u : 2u);
				SDL_GPUBufferCreateInfo bi {};
				bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
				bi.size = max<uint32_t>(1, bytes);
				SDL_GPUBuffer* buf = SDL_CreateGPUBuffer(_device, &bi);
				if (!buf) return renderHandle{};
				bufferRecord rec;
				rec.buffer = buf;
				rec.isIndex = true;
				rec.isTransient = true;
				rec.index32 = (flags & BufferIndex32) != 0;
				rec.size = bi.size;
				rec.transientData.assign(bi.size, uint8_t(0));
				_buffers[idx] = rec;
				return renderHandle{idx};
			}
			void updateVertexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				auto it = _buffers.find(h.idx);
				if (it == _buffers.end() || !data || !size) return;
				auto& rec = it->second;
				if (rec.isTransient) {
					if (start + size > rec.transientData.size()) return;
					memcpy(rec.transientData.data() + start, data, size);
					rec.transientDirty = true;
					return;
				}
				pendingBufferUpload(h.idx, data, size, start);
			}
			void updateIndexBuffer(renderHandle h, const void* data,
				uint32_t size, uint32_t start, uint32_t) override {
				auto it = _buffers.find(h.idx);
				if (it == _buffers.end() || !data || !size) return;
				auto& rec = it->second;
				if (rec.isTransient) {
					if (start + size > rec.transientData.size()) return;
					memcpy(rec.transientData.data() + start, data, size);
					rec.transientDirty = true;
					return;
				}
				pendingBufferUpload(h.idx, data, size, start);
			}
			void destroyBuffer(renderHandle h) override {
				auto it = _buffers.find(h.idx);
				if (it == _buffers.end()) return;
				SDL_ReleaseGPUBuffer(_device, it->second.buffer);
				_buffers.erase(it);
			}

			// ---- textures ---------------------------------------------------
			renderHandle createTextureLike(SDL_GPUTextureType type,
				uint32_t w, uint32_t h, uint32_t d, uint8_t numMips,
				uint16_t numLayers, texFormat f, uint64_t flags,
				const void* data, uint32_t size) {
				const SDL_GPUTextureFormat format = toSDLFormat(f, false);
				if (format == SDL_GPU_TEXTUREFORMAT_INVALID) {
					fprintf(stderr,
						"[sdlgpu] unsupported texture format %d\n",
						int(f));
					return renderHandle{};
				}
				SDL_GPUTextureCreateInfo ci {};
				ci.type = type;
				ci.format = format;
				ci.width = w;
				ci.height = h;
				ci.layer_count_or_depth =
					type == SDL_GPU_TEXTURETYPE_3D ? d
					: type == SDL_GPU_TEXTURETYPE_CUBE ? 6
					: max<uint32_t>(1, numLayers);
				ci.num_levels = max<uint32_t>(1, numMips);
				ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
				SDL_GPUTexture* tex = SDL_CreateGPUTexture(_device, &ci);
				if (!tex) {
					fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
					return renderHandle{};
				}
				const uint16_t idx = _nextTexture++;
				textureRecord rec;
				rec.texture = tex;
				rec.format = format;
				rec.type = type;
				rec.width = w;
				rec.height = h;
				rec.depth = type == SDL_GPU_TEXTURETYPE_3D ? d : 1;
				rec.numMips = max<uint8_t>(1, numMips);
				rec.numLayers = type == SDL_GPU_TEXTURETYPE_CUBE
					? 6 : max<uint16_t>(1, numLayers);
				rec.cube = type == SDL_GPU_TEXTURETYPE_CUBE;
				rec.samplerFlags = uint32_t(flags & 0xFFFFFFFF);
				_textures[idx] = rec;
				if (data && size)
					pendingTextureUpload(idx, data, size);
				return renderHandle{idx};
			}

			void pendingTextureUpload(uint16_t idx, const void* data,
				uint32_t size) {
				SDL_GPUTransferBufferCreateInfo ti {};
				ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
				ti.size = size ? size : 1;
				SDL_GPUTransferBuffer* tb =
					SDL_CreateGPUTransferBuffer(_device, &ti);
				if (!tb) return;
				auto* dst = SDL_MapGPUTransferBuffer(_device, tb, false);
				memcpy(dst, data, size);
				SDL_UnmapGPUTransferBuffer(_device, tb);
				_pendingTexUploads.push_back({idx, tb});
			}

			// bgfx's calcNumMips: a full chain when hasMips (1 + log2 of
			// the max side), one mip otherwise.
			static uint8_t calcMipCount(bool hasMips, uint32_t w, uint32_t h,
				uint32_t d) {
				if (!hasMips) return 1;
				uint32_t side = max(max(w, h), d);
				uint8_t levels = 1;
				while (side > 1) {
					side >>= 1;
					++levels;
				}
				return levels;
			}

			renderHandle createTexture2D(uint16_t w, uint16_t h,
				bool hasMips, uint16_t numLayers, texFormat f,
				uint64_t flags, const void* data, uint32_t size) override {
				return createTextureLike(SDL_GPU_TEXTURETYPE_2D, w, h, 1,
					calcMipCount(hasMips, w, h, 1), numLayers, f, flags,
					data, size);
			}
			renderHandle createTextureCube(uint16_t size, bool hasMips,
				uint16_t numLayers, texFormat f, uint64_t flags,
				const void* data, uint32_t size_) override {
				return createTextureLike(SDL_GPU_TEXTURETYPE_CUBE, size,
					size, 1, calcMipCount(hasMips, size, size, 1),
					max<uint16_t>(6, numLayers), f, flags, data, size_);
			}
			renderHandle createTexture3D(uint16_t w, uint16_t h, uint16_t d,
				bool hasMips, texFormat f, uint64_t flags, const void* data,
				uint32_t size) override {
				return createTextureLike(SDL_GPU_TEXTURETYPE_3D, w, h, d,
					calcMipCount(hasMips, w, h, d), 1, f, flags, data,
					size);
			}
			// updateTexture: the facade routes cube faces through here;
			// the engine's update calls always send whole mips today.
			void updateTexture(renderHandle h, uint8_t side, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)side;
				(void)mip;
				pendingTextureUpload(h.idx, data, size);
			}
			void updateTexture2D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)mip;
				pendingTextureUpload(h.idx, data, size);
			}
			void updateTexture3D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)mip;
				pendingTextureUpload(h.idx, data, size);
			}
			void destroyTexture(renderHandle h) override {
				auto it = _textures.find(h.idx);
				if (it == _textures.end()) return;
				if (it->second.sampler)
					SDL_ReleaseGPUSampler(_device, it->second.sampler);
				SDL_ReleaseGPUTexture(_device, it->second.texture);
				_textures.erase(it);
			}
			bool readTexture(renderHandle h, void* data, uint8_t mip)
				override {
				// Blocking readback: a download transfer buffer + a copy
				// pass + device idle. Readbacks are rare (tests, saves),
				// so the simple path is fine.
				auto it = _textures.find(h.idx);
				if (it == _textures.end() || !data) return false;
				auto& rec = it->second;
				const uint8_t targetMip =
					min<uint8_t>(mip, uint8_t(rec.numMips - 1));
				const uint32_t mw = max(1u, rec.width >> targetMip);
				const uint32_t mh = max(1u, rec.height >> targetMip);
				const uint32_t md =
					rec.type == SDL_GPU_TEXTURETYPE_3D
						? max(1u, rec.depth >> targetMip)
						: 1;
				const uint32_t mipSize =
					toMipBytes(rec.format, mw, mh, md);
				SDL_GPUTransferBufferCreateInfo ti {};
				ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
				ti.size = mipSize;
				SDL_GPUTransferBuffer* tb =
					SDL_CreateGPUTransferBuffer(_device, &ti);
				if (!tb) return false;

				SDL_GPUCommandBuffer* cmd =
					SDL_AcquireGPUCommandBuffer(_device);
				if (!cmd) {
					SDL_ReleaseGPUTransferBuffer(_device, tb);
					return false;
				}
				SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
				SDL_GPUTextureRegion region {};
				region.texture = rec.texture;
				region.mip_level = targetMip;
				region.w = mw;
				region.h = mh;
				region.d = md;
				SDL_GPUTextureTransferInfo dstInfo {};
				dstInfo.transfer_buffer = tb;
				// Zero pitch/rows = SDL's densest packing.
				SDL_DownloadFromGPUTexture(cp, &region, &dstInfo);
				SDL_EndGPUCopyPass(cp);
				SDL_SubmitGPUCommandBuffer(cmd);
				SDL_WaitForGPUIdle(_device);

				auto* mapped = SDL_MapGPUTransferBuffer(_device, tb, false);
				memcpy(data, mapped, mipSize);
				SDL_UnmapGPUTransferBuffer(_device, tb);
				SDL_ReleaseGPUTransferBuffer(_device, tb);
				return true;
			}
			void* directAccessPtr(renderHandle) override {
				// bgfx's GL direct access has no SDL_GPU counterpart.
				return nullptr;
			}

			// ---- frame lifecycle -----------------------------------------
			bool beginFrame() override {
				// Transients recycle with the frame (bgfx's ring
				// semantics): last frame's transient records die here.
				for (auto it = _buffers.begin(); it != _buffers.end();) {
					if (it->second.isTransient) {
						SDL_ReleaseGPUBuffer(_device, it->second.buffer);
						it = _buffers.erase(it);
					} else
						++it;
				}
				return _device != nullptr;
			}

			bool endFrame() override {
				// 4c replaces the second half with the view-pass walk;
				// today the frame is: uploads, then a cleared present.
				if (!_device) return false;
				SDL_GPUCommandBuffer* cmd =
					SDL_AcquireGPUCommandBuffer(_device);
				if (!cmd) return false;
				runPendingUploads(cmd);
				if (!_offscreen && _window) {
					SDL_GPUTexture* swapchain = nullptr;
					if (SDL_WaitAndAcquireGPUSwapchainTexture(
							cmd, _window, &swapchain, nullptr, nullptr)
						&& swapchain) {
						SDL_GPUColorTargetInfo target {};
						target.texture = swapchain;
						target.clear_color.r =
							float((_clearColor >> 24) & 0xFF) / 255.0f;
						target.clear_color.g =
							float((_clearColor >> 16) & 0xFF) / 255.0f;
						target.clear_color.b =
							float((_clearColor >> 8) & 0xFF) / 255.0f;
						target.clear_color.a =
							float(_clearColor & 0xFF) / 255.0f;
						target.load_op = (_clearFlags & ClearColor)
							? SDL_GPU_LOADOP_CLEAR
							: SDL_GPU_LOADOP_LOAD;
						target.store_op = SDL_GPU_STOREOP_STORE;
						SDL_GPURenderPass* pass =
							SDL_BeginGPURenderPass(cmd, &target, 1,
								nullptr);
						if (pass) SDL_EndGPURenderPass(pass);
					}
				}
				SDL_SubmitGPUCommandBuffer(cmd);
				// Transfer buffers ride the submitted copy pass; SDL frees
				// them safely, so a release here only queues the free.
				for (auto& up : _pendingUploads)
					SDL_ReleaseGPUTransferBuffer(_device, up.tb);
				_pendingUploads.clear();
				for (auto& up : _pendingTexUploads)
					SDL_ReleaseGPUTransferBuffer(_device, up.tb);
				_pendingTexUploads.clear();
				return true;
			}

			// The frame's copy pass: static/dynamic buffer uploads, the
			// whole texture blocks, dirty transients. Runs inside the
			// frame's command buffer before any view pass.
			void runPendingUploads(SDL_GPUCommandBuffer* cmd) {
				bool transientDirty = false;
				for (auto& [_, rec] : _buffers)
					if (rec.isTransient && rec.transientDirty) {
						transientDirty = true;
						break;
					}
				if (_pendingUploads.empty()
					&& _pendingTexUploads.empty() && !transientDirty)
					return;
				auto* cp = SDL_BeginGPUCopyPass(cmd);
				vector<SDL_GPUTransferBuffer*> toRelease;
				for (auto& up : _pendingUploads) {
					auto it = _buffers.find(up.buffer);
					if (it == _buffers.end()) {
						toRelease.push_back(up.tb);
						continue;
					}
					SDL_GPUTransferBufferLocation src {};
					src.transfer_buffer = up.tb;
					src.offset = 0;
					SDL_GPUBufferRegion dst {};
					dst.buffer = it->second.buffer;
					dst.offset = up.offset;
					dst.size = up.size;
					SDL_UploadToGPUBuffer(cp, &src, &dst, up.offset == 0);
					toRelease.push_back(up.tb);
				}
				_pendingUploads.clear();
				for (auto& [_, rec] : _buffers) {
					if (!rec.isTransient || !rec.transientDirty) continue;
					SDL_GPUTransferBufferCreateInfo ti {};
					ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
					ti.size = uint32_t(rec.transientData.size());
					SDL_GPUTransferBuffer* tb =
						SDL_CreateGPUTransferBuffer(_device, &ti);
					if (!tb) continue;
					auto* srcMem =
						SDL_MapGPUTransferBuffer(_device, tb, false);
					memcpy(srcMem, rec.transientData.data(),
						rec.transientData.size());
					SDL_UnmapGPUTransferBuffer(_device, tb);
					SDL_GPUTransferBufferLocation src {};
					src.transfer_buffer = tb;
					SDL_GPUBufferRegion dst {};
					dst.buffer = rec.buffer;
					dst.size = rec.size;
					SDL_UploadToGPUBuffer(cp, &src, &dst, false);
					toRelease.push_back(tb);
					rec.transientDirty = false;
				}
				for (auto& up : _pendingTexUploads) {
					auto it = _textures.find(up.texture);
					if (it == _textures.end()) {
						toRelease.push_back(up.tb);
						continue;
					}
					auto& rec = it->second;
					SDL_GPUTextureTransferInfo srcInfo {};
					srcInfo.transfer_buffer = up.tb;
					// The raw block's layout: mip 0 first, then the rest
					// of the chain; layers/faces ride each mip's block.
					uint32_t offset = 0;
					for (uint32_t mip = 0; mip < rec.numMips; ++mip) {
						const uint32_t mw = max(1u, rec.width >> mip);
						const uint32_t mh = max(1u, rec.height >> mip);
						if (rec.numLayers == 1
							&& (rec.type != SDL_GPU_TEXTURETYPE_3D
								|| (rec.depth >> mip) < 2)) {
							SDL_GPUTextureRegion dst {};
							dst.texture = rec.texture;
							dst.mip_level = mip;
							dst.w = mw;
							dst.h = mh;
							dst.d = rec.type == SDL_GPU_TEXTURETYPE_3D
								? max(1u, rec.depth >> mip) : 1u;
							srcInfo.offset = offset;
							srcInfo.pixels_per_row = mw;
							srcInfo.rows_per_layer = mh;
							SDL_UploadToGPUTexture(cp, &srcInfo, &dst,
								false);
							offset += toMipBytes(rec.format, mw, mh,
								dst.d);
							continue;
						}
						// Layers (2D arrays, cube faces) or depth slices:
						// each layer's block back-to-back per mip.
						const uint32_t span =
							rec.type == SDL_GPU_TEXTURETYPE_3D
								? 1u : rec.numLayers;
						for (uint32_t layer = 0; layer < span; ++layer) {
							SDL_GPUTextureRegion dst {};
							dst.texture = rec.texture;
							dst.mip_level = mip;
							dst.layer = layer;
							dst.w = mw;
							dst.h = mh;
							dst.d = 1;
							srcInfo.offset = offset;
							srcInfo.pixels_per_row = mw;
							srcInfo.rows_per_layer = mh;
							SDL_UploadToGPUTexture(cp, &srcInfo, &dst,
								false);
							offset += toMipBytes(rec.format, mw, mh, 1);
						}
					}
					toRelease.push_back(up.tb);
				}
				_pendingTexUploads.clear();
				SDL_EndGPUCopyPass(cp);
				for (auto* tb : toRelease)
					SDL_ReleaseGPUTransferBuffer(_device, tb);
			}

			// ---- the draw surface (4c wires views/pipelines) -------------
			void setVertexBuffer(uint8_t, renderHandle, uint32_t, uint32_t)
				override {}
			void setIndexBuffer(renderHandle, uint32_t, uint32_t) override {}
			void submit(uint8_t, renderHandle, uint32_t, uint16_t) override {}
			void submitQuery(uint8_t, renderHandle, renderHandle, uint32_t,
				uint16_t) override {}
			void submitIndirect(uint8_t, renderHandle, renderHandle,
				uint16_t, uint16_t, uint32_t, uint16_t) override {}
			void dispatch(uint8_t, renderHandle, uint32_t, uint32_t,
				uint32_t, uint16_t) override {}
			void dispatchIndirect(uint8_t, renderHandle, renderHandle,
				uint16_t, uint16_t, uint16_t) override {}
			void setState(uint64_t, uint32_t) override {}
			void setStencil(uint32_t, uint32_t) override {}
			void setTransform(const void*) override {}
			void setTexture(uint8_t, const char*, renderHandle, uint32_t)
				override {}
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
			renderHandle createIndirectBuffer(const void*, uint32_t)
				override {
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
				uint16_t, renderHandle, uint8_t, uint16_t, uint16_t,
				uint16_t, uint16_t, uint16_t, uint16_t) override {}
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