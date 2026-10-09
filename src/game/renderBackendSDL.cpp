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

#include <bx/math.h>
#include <bx/float4x4_t.h>
#include <bx/allocator.h>
#include <bx/readerwriter.h>
#include <bimg/bimg.h>
#include <bimg/encode.h>

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
			bool isArray = false;  // the decl's [n] survives (u_Lights[1])
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
			// The SDL sampler slot per sampler name: SDL requires every
			// declared sampler bound and slots contiguous from zero, so
			// slot = the sampler's index in the (stripped) list; the
			// facade's stage numbers map through the NAME.
			map<string, uint32_t> samplerSlots;
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
			// The gold format (the update path converts 16-bit RGBA).
			texFormat goldFormat = texFormat::RGBA8;
		};

		// --- format maps ----------------------------------------------------
		// gold's texFormat enum mirrors bgfx's ordering for the cast; the
		// SDL map is explicit so drift shows as INVALID instead of wrong
		// bytes. Compressed formats land here when their block math is
		// wired.
		// The engine's texture contract: the caller's format number rides
		// bimg 5.x's own enum values (the image facade's decodes emit
		// them; the bgfx backend passes them straight through — its cast
		// lands on the same-named format). The SDL map reads the same
		// numbers; the bytes never change shape.
		binary sdlTextureBytes(texFormat, const void* data, uint32_t size) {
			binary out;
			if (!data || !size) return out;
			out.assign((const uint8_t*)data,
				(const uint8_t*)data + size);
			return out;
		}

		SDL_GPUTextureFormat toSDLFormat(texFormat f, bool srgb) {
			switch (f) {
			case texFormat::RGBA8:
				return srgb
					? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
					: SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			case texFormat::RGBA16:
				// The data converts to 8-bit before the upload (SDL has
				// no UNORM 16-bit RGBA texture); the mapped format
				// answers the converted bytes.
				return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
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
			if (stem.rfind("texcoord", 0) == 0
				&& stem.size() > 8
				&& isdigit((unsigned char)stem[8]))
				return uint32_t(vertexAttrib::TexCoord0) +
					uint32_t(stem[8] - '0');
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
					entry.isArray = bracket != string::npos;
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
					// Unused io drops: Vulkan requires the pipeline's
					// descriptors to cover every DECLARED vertex input,
					// and the engine's meshes only ship data for the
					// attributes they actually load.
					string bodyless;
					for (auto& other : lines) {
						if (other == line) continue;
						bodyless += other + "\n";
					}
					if (!identifierUsed(bodyless, name)) continue;
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
					block.push_back(
						"    " + string(type) + " " + entry.name +
						(entry.isArray || entry.num > 1
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
		 uint16_t _width = 0, _height = 0;
		 bool _offscreen = false;
		 // The config's rgba: the fresh views' default clear.
		 uint32_t _defaultClearColor = 0x6ab0deff;
		 float _defaultClearDepth = 1.0f;

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
				_defaultClearColor = config.getUInt32(
					"rgba", 0x6ab0deff);
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
					for (auto& [_, pipeline] : _pipelines)
						SDL_ReleaseGPUGraphicsPipeline(_device,
							pipeline);
					for (auto& [_, tex] : _depthTextures)
						SDL_ReleaseGPUTexture(_device, tex);
					if (_fillerTex)
						SDL_ReleaseGPUTexture(_device, _fillerTex);
					if (_fillerSmp)
						SDL_ReleaseGPUSampler(_device, _fillerSmp);
					_fillerTex = nullptr;
					_fillerSmp = nullptr;
					_views.clear();
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
				// The SDL sampler slots: the (stripped) list's order.
				const uint16_t idx = _nextStage++;
				_stages[idx] = rec;
				_stages[idx].shader = shader;
				auto& stored = _stages[idx];
				for (uint32_t s = 0; s < stored.samplers.size(); ++s)
					stored.samplerSlots[stored.samplers[s].name] = s;
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
				// (what the GL path compiles today). The bgfx shader
				// library (bgfx_shader.sh and friends) is this backend's
				// own include, ahead of the caller's dirs.
				vector<string> args {
					GOLD_SHADER_COMPILER, "-f", path,
					"--type", vertex ? "vertex" : "fragment",
					"--platform", "linux",
					"--profile", "330",
					"-i", GOLD_BGFX_SHADER_INCLUDE,
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
				if (w == 0 || h == 0
					|| (type == SDL_GPU_TEXTURETYPE_3D && d == 0)) {
					// The callers guard; a zero-dim create would fail the
					// device's validation. Skip loudly.
					fprintf(stderr,
						"[sdlgpu] text2D: zero extent %ux%ux%u (fmt %d)\n",
						w, h, d, int(f));
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
					fprintf(stderr,
						"[sdlgpu] texture %ux%u fmt %d: %s\n", w, h,
						int(f), SDL_GetError());
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
				rec.goldFormat = f;
				_textures[idx] = rec;
				if (data && size) {
					auto converted = sdlTextureBytes(f, data, size);
					pendingTextureUpload(idx, converted.data(),
						uint32_t(converted.size()));
				}
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
			void updateTextureConverted(renderHandle h, const void* data,
				uint32_t size) {
				auto it = _textures.find(h.idx);
				if (it == _textures.end()) return;
				auto converted = sdlTextureBytes(
					it->second.goldFormat, data, size);
				pendingTextureUpload(h.idx, converted.data(),
					uint32_t(converted.size()));
			}
			void updateTexture(renderHandle h, uint8_t side, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)side;
				(void)mip;
				updateTextureConverted(h, data, size);
			}
			void updateTexture2D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)mip;
				updateTextureConverted(h, data, size);
			}
			void updateTexture3D(renderHandle h, uint8_t mip,
				const void* data, uint32_t size) override {
				(void)mip;
				updateTextureConverted(h, data, size);
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
				fprintf(stderr, "[sdlgpu] read dl: tex w %u h %u mip %u\n",
					mw, mh, targetMip);
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
				SDL_GPUTexture* swapchain = nullptr;
				uint32_t swapW = 0, swapH = 0;
				if (!_offscreen && _window) {
					SDL_WaitAndAcquireGPUSwapchainTexture(cmd, _window,
						&swapchain, &swapW, &swapH);
				}
				// A screenshot frame renders into an owned transfer-
				// source RT (the swapchain cannot be read back) and
				// blits to the swapchain after the passes.
				SDL_GPUTexture* renderTarget = swapchain;
				SDL_GPUTextureFormat rtFormat = swapchain
					? SDL_GetGPUSwapchainTextureFormat(_device, _window)
					: _format;
				SDL_GPUTexture* shotTex = nullptr;
				if (!_shots.empty() && swapchain) {
					SDL_GPUTextureCreateInfo ci {};
					ci.type = SDL_GPU_TEXTURETYPE_2D;
					ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
					ci.width = swapW;
					ci.height = swapH;
					ci.layer_count_or_depth = 1;
					ci.num_levels = 1;
					// (SDL's backend makes every texture transfer-src
					// capable; no usage flag exists.)
					ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
						| SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
					renderTarget = SDL_CreateGPUTexture(_device, &ci);
					rtFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
					shotTex = renderTarget;
				}
				renderViews(cmd, renderTarget, rtFormat, swapW, swapH);
				if (shotTex) {
					// The present: blit the RT into the swapchain.
					SDL_GPUBlitInfo blit {};
					blit.source.texture = shotTex;
					blit.source.w = swapW;
					blit.source.h = swapH;
					blit.destination.texture = swapchain;
					blit.destination.w = swapW;
					blit.destination.h = swapH;
					blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
					blit.filter = SDL_GPU_FILTER_LINEAR;
					SDL_BlitGPUTexture(cmd, &blit);
					// The readbacks ride this frame's content; queue them
					// for the post-submit wait.
					for (auto& shot : _shots)
						_pendingShots.push_back(
							{shot.fb == 0xFFFF ? uint16_t(0)
											   : shot.fb,
							 shot.path, shotTex, swapW, swapH});
					_shots.clear();
				}
				SDL_SubmitGPUCommandBuffer(cmd);
				if (shotTex || !_pendingShots.empty())
					SDL_WaitForGPUIdle(_device);
				// Screenshots: readbacks of the frame's own textures.
				drainPendingShots();
				if (shotTex)
					SDL_ReleaseGPUTexture(_device, shotTex);
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

			// The readback rows of a finished shot: texture + size +
			// destination. The command buffer (the frame's) finished.
			struct shotReadback {
				uint16_t fbOrTexture;   // 0 = the shot frame's texture
				string path;
				SDL_GPUTexture* texture;
				uint32_t w, h;
			};
			vector<shotReadback> _pendingShots;

			void drainPendingShots() {
				for (auto& shot : _pendingShots) {
					SDL_GPUTexture* src = shot.texture;
					if (shot.fbOrTexture != 0) {
						auto fbIt = _framebuffers.find(shot.fbOrTexture);
						if (fbIt == _framebuffers.end()
							|| fbIt->second.colors.empty()) {
							fprintf(stderr,
								"[sdlgpu] shot: no color\n");
							continue;
						}
						auto texIt = _textures.find(
							fbIt->second.colors[0]);
						if (texIt == _textures.end()) continue;
						src = texIt->second.texture;
						shot.w = texIt->second.width;
						shot.h = texIt->second.height;
					}
					if (!src || !shot.w || !shot.h) continue;
					const uint32_t bytes =
						toMipBytes(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
							shot.w, shot.h, 1);
					SDL_GPUTransferBufferCreateInfo ti {};
					ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
					ti.size = bytes;
					SDL_GPUTransferBuffer* tb =
						SDL_CreateGPUTransferBuffer(_device, &ti);
					if (!tb) return;
					{
						SDL_GPUCommandBuffer* cmd =
							SDL_AcquireGPUCommandBuffer(_device);
						if (!cmd) {
							SDL_ReleaseGPUTransferBuffer(_device, tb);
							return;
						}
							fprintf(stderr,
							"[sdlgpu] drain dl: %u tex? %d w %u h %u\n",
							shot.fbOrTexture, src != nullptr, shot.w,
							shot.h);
						SDL_GPUCopyPass* cp =
							SDL_BeginGPUCopyPass(cmd);
						SDL_GPUTextureRegion region {};
						region.texture = src;
						region.w = shot.w;
						region.h = shot.h;
						region.d = 1;
						SDL_GPUTextureTransferInfo dstInfo {};
						dstInfo.transfer_buffer = tb;
						SDL_DownloadFromGPUTexture(cp, &region,
							&dstInfo);
						SDL_EndGPUCopyPass(cp);
						SDL_SubmitGPUCommandBuffer(cmd);
					}
					SDL_WaitForGPUIdle(_device);
					auto* mapped =
						SDL_MapGPUTransferBuffer(_device, tb, false);
					// The readback rows are top-down; the PNG wants the
					// same order (no flip).
					bx::DefaultAllocator allocator;
					auto block = bx::MemoryBlock(&allocator);
					auto writer = bx::MemoryWriter(&block);
					auto error = bx::Error();
					const auto pngSize = bimg::imageWritePng(&writer,
						uint16_t(shot.w), uint16_t(shot.h), shot.w * 4,
						const_cast<void*>(mapped),
						bimg::TextureFormat::RGBA8, false, &error);
					SDL_UnmapGPUTransferBuffer(_device, tb);
					SDL_ReleaseGPUTransferBuffer(_device, tb);
					if (error.isOk()) {
						std::ofstream out(shot.path,
							std::ofstream::binary);
						out.write((const char*)block.more(),
							(std::streamsize)pngSize);
						fprintf(stderr, "[sdlgpu] shot %s (%ux%u)\n",
							shot.path.c_str(), shot.w, shot.h);
					} else
						fprintf(stderr, "[sdlgpu] shot png failed\n");
				}
				_pendingShots.clear();
			}

			// One blocking readback per shot: the present submit finished
			// the frame; the target texture holds the rendered pixels now.
			void retireShots() {
				while (!_shots.empty()) {
					auto shot = _shots.back();
					_shots.pop_back();
					SDL_GPUTexture* tex = nullptr;
					uint32_t w = 0, h = 0;
					SDL_GPUTextureFormat format;
					if (shot.fb != 0xFFFF) {
						auto fbIt = _framebuffers.find(shot.fb);
						if (fbIt == _framebuffers.end()
							|| fbIt->second.colors.empty()) {
							fprintf(stderr, "[sdlgpu] shot: no color\n");
							continue;
						}
						auto texIt = _textures.find(
							fbIt->second.colors[0]);
						if (texIt != _textures.end()) {
							tex = texIt->second.texture;
							w = texIt->second.width;
							h = texIt->second.height;
							format = texIt->second.format;
						}
					} else {
						// The back buffer: recreate the last swapchain
						// texture is impossible — read the WINDOW's size
						// and re-acquire in a fresh command buffer.
						tex = nullptr;
					}
					SDL_GPUCommandBuffer* cmd =
						SDL_AcquireGPUCommandBuffer(_device);
					if (!cmd) continue;
					SDL_GPUTransferBufferCreateInfo ti {};
					SDL_GPUTextureRegion region {};
					if (shot.fb == 0xFFFF) {
						// A fresh swapchain frame: acquire + submit it so
						// the readout is the JUST-rendered content.
						SDL_WaitAndAcquireGPUSwapchainTexture(cmd,
							_window, &tex, &w, &h);
						format = tex
							? SDL_GetGPUSwapchainTextureFormat(
								_device, _window)
							: SDL_GPU_TEXTUREFORMAT_INVALID;
					}
					if (!tex) {
						SDL_SubmitGPUCommandBuffer(cmd);
						continue;
					}
					uint32_t bytes = toMipBytesForReadback(format, w, h);
					ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
					ti.size = bytes;
					SDL_GPUTransferBuffer* tb =
						SDL_CreateGPUTransferBuffer(_device, &ti);
					if (!tb) continue;
					SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
					region.texture = tex;
					region.w = w;
					region.h = h;
					region.d = 1;
					SDL_GPUTextureTransferInfo dstInfo {};
					dstInfo.transfer_buffer = tb;
					SDL_DownloadFromGPUTexture(cp, &region, &dstInfo);
					SDL_EndGPUCopyPass(cp);
					SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
					SDL_WaitForGPUIdle(_device);
					auto* mapped =
						SDL_MapGPUTransferBuffer(_device, tb, false);
					// PNG via bimg (the bgfx shot path's same writer).
					writePng(shot.path, mapped, w, h, format);
					SDL_UnmapGPUTransferBuffer(_device, tb);
					SDL_ReleaseGPUTransferBuffer(_device, tb);
				}
			}

			static uint32_t toMipBytesForReadback(
				SDL_GPUTextureFormat format, uint32_t w, uint32_t h) {
				return toMipBytes(format, w, h, 1);
			}

			void writePng(const string& path, const void* pixels,
				uint32_t w, uint32_t h, SDL_GPUTextureFormat format) {
				(void)format;
				// The bimg PNG writer, like the bgfx callbacks' shot path.
				bx::DefaultAllocator allocator;
				auto block = bx::MemoryBlock(&allocator);
				auto writer = bx::MemoryWriter(&block);
				auto error = bx::Error();
				const auto size = bimg::imageWritePng(&writer,
					uint16_t(w), uint16_t(h), w * 4,
					const_cast<void*>(pixels),
					bimg::TextureFormat::Enum(
						bimg::TextureFormat::RGBA8), false, &error);
				std::ofstream out(path, std::ofstream::binary);
				if (!out.is_open()) {
					fprintf(stderr, "[sdlgpu] shot: open %s failed\n",
					path.c_str());
					return;
				}
				out.write((const char*)block.more(), size);
				fprintf(stderr, "[sdlgpu] shot %s (%ux%u)\n",
					path.c_str(), w, h);
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
				if (_fillerUploadPending && _fillerTex) {
					SDL_GPUTransferBufferCreateInfo ti {};
					ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
					ti.size = 4;
					SDL_GPUTransferBuffer* tb =
						SDL_CreateGPUTransferBuffer(_device, &ti);
					if (tb) {
						auto* mem =
							SDL_MapGPUTransferBuffer(_device, tb, false);
						const uint8_t white[4] = {255, 255, 255, 255};
						memcpy(mem, white, 4);
						SDL_UnmapGPUTransferBuffer(_device, tb);
						SDL_GPUTextureTransferInfo srcInfo {};
						srcInfo.transfer_buffer = tb;
						SDL_GPUTextureRegion dst {};
						dst.texture = _fillerTex;
						dst.w = 1;
						dst.h = 1;
						SDL_UploadToGPUTexture(cp, &srcInfo, &dst,
							false);
						toRelease.push_back(tb);
						_fillerUploadPending = false;
					}
				}
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

						// ---- the draw surface ----------------------------------------
			// Views, per-draw state, and pipelines. bgfx's frame model:
			// the encoder-side calls accrue; submit() captures a draw with
			// its state; endFrame processes views in ascending id order,
			// one pass each, drawing the queue, then presents.

			/** One draw, captured at submit (the per-draw state snapshot). */
			struct drawItem {
				uint16_t program = 0xFFFF;
				uint64_t state = 0;
				uint32_t stateColor = 0;
				uint32_t stencilF = 0, stencilB = 0;
				bool hasModel = false;
				float model[16];
				bool hasIndex = false;
				uint16_t indexBuffer = 0xFFFF;
				uint32_t indexFirst = 0, indexCount = 0;
				vector<pair<uint16_t, uint16_t>> vBinds;  // stream, buffer
				vector<uint32_t> vStarts, vCounts;
				// (slot -> texture) binds; slot = the SDL sampler slot.
				vector<pair<uint16_t, uint16_t>> texBinds;
				uint32_t depth = 0;
			};

			struct viewRecord {
				bool touched = false;
				uint16_t clearFlags = ClearNone;
				uint32_t clearColor = 0;
				float clearDepth = 1.0f;
				uint8_t clearStencil = 0;
				uint16_t x = 0, y = 0, w = 0, h = 0;
				float view[16], proj[16];
				bool hasTransform = false;
				uint16_t fb = 0xFFFF;
				vector<drawItem> draws;
			};
			map<uint8_t, viewRecord> _views;

			viewRecord& ensureView(uint8_t view) {
				auto it = _views.find(view);
				if (it != _views.end()) return it->second;
				viewRecord vr;
				vr.clearColor = _defaultClearColor;
				vr.clearDepth = _defaultClearDepth;
				vr.w = _width;
				vr.h = _height;
				return _views[view] = vr;
			}

			// The rolling per-draw state (bgfx's persist-until-changed
			// semantics; everything re-set per draw by the engine).
			uint64_t _stateBits = 0;
			uint32_t _stateColor = 0;
			uint32_t _stencilF = 0, _stencilB = 0;
			bool _hasModel = false;
			float _modelMtx[16];
			vector<pair<uint16_t, uint16_t>> _vBinds;
			vector<uint32_t> _vStarts, _vCounts;
			bool _hasIndex = false;
			uint16_t _indexBuffer = 0xFFFF;
			uint32_t _indexFirst = 0, _indexCount = 0;
			vector<pair<uint16_t, uint16_t>> _texUniformBinds;

			struct pipelineKey {
				uint16_t program;
				uint64_t state;
				uint32_t stencilF, stencilB;
				SDL_GPUTextureFormat color, depth;
				bool operator<(const pipelineKey& o) const {
					return memcmp(this, &o, sizeof(*this)) < 0;
				}
			};
			map<pipelineKey, SDL_GPUGraphicsPipeline*> _pipelines;

			struct framebufferRecord {
				vector<uint16_t> colors;  // gold texture handles
				uint16_t depth = 0xFFFF;
				vector<uint16_t> owned;   // descriptor-created textures
			};
			map<uint16_t, framebufferRecord> _framebuffers;
			uint16_t _nextFramebuffer = 1;
			// Per-size D32 depth textures for swapchain passes.
			map<pair<uint32_t, uint32_t>, SDL_GPUTexture*> _depthTextures;

			// gold's clear/view/draw state --------------------------------------------------
			void touch(uint8_t view) override { ensureView(view).touched = true; }

			void viewClear(uint8_t view, uint16_t flags, uint32_t rgba,
				float depth, uint8_t stencil) override {
				auto& vr = ensureView(view);
				vr.touched = true;
				vr.clearFlags = flags;
				vr.clearColor = rgba;
				vr.clearDepth = depth;
				vr.clearStencil = stencil;
			}
			void viewRect(uint8_t view, uint16_t x, uint16_t y, uint16_t w,
				uint16_t h) override {
				auto& vr = ensureView(view);
				vr.x = x; vr.y = y;
				vr.w = w ? w : _width;
				vr.h = h ? h : _height;
			}
			void viewTransform(uint8_t view, const void* viewMtx,
				const void* projMtx) override {
				auto& vr = ensureView(view);
				if (viewMtx) {
					memcpy(vr.view, viewMtx, 64);
					vr.hasTransform = true;
				}
				if (projMtx) memcpy(vr.proj, projMtx, 64);
			}
			void setViewFrameBuffer(uint8_t view, renderHandle fb) override {
				ensureView(view).fb = fb.idx;
			}

			void setState(uint64_t state, uint32_t rgba) override {
				_stateBits = state;
				_stateColor = rgba;
			}
			void setStencil(uint32_t fstencil, uint32_t bstencil) override {
				_stencilF = fstencil;
				_stencilB = bstencil;
			}
			void setTransform(const void* mtx) override {
				if (!mtx) {
					_hasModel = false;
					return;
				}
				memcpy(_modelMtx, mtx, 64);
				_hasModel = true;
			}
			// Sampler binds: the item carries (uniform index, texture); the
			// draw resolves the SDL slot via the stage's sampler name
			// table (SDL wants every declared sampler bound, with slots
			// contiguous from zero in the stage's own order).
			void setTexture(uint8_t, const char*, renderHandle, uint32_t)
				override {
				// The named path has no facade users today (bindTexture
				// goes through a registered uniform); bgfx-only.
			}
			// The facade binds through a registered sampler-uniform
			// (bindTexture): the uniform's NAME is what the stage's
			// sampler table knows.
			void setTextureUniform(uint8_t stage, renderHandle uniform,
				renderHandle tex, uint32_t flags) override {
				(void)stage;
				(void)flags;
				if (!uniform.valid() || !tex.valid()) return;
				_texUniformBinds.push_back({uniform.idx, tex.idx});
			}
			void setVertexBuffer(uint8_t stream, renderHandle h,
				uint32_t start, uint32_t num) override {
				// The stream's position in the list is its binding slot.
				for (size_t i = 0; i < _vBinds.size(); ++i)
					if (_vBinds[i].first == stream) {
						_vBinds.erase(_vBinds.begin() + i);
						_vStarts.erase(_vStarts.begin() + i);
						_vCounts.erase(_vCounts.begin() + i);
						break;
					}
				if (!h.valid()) return;
				_vBinds.push_back({stream, h.idx});
				_vStarts.push_back(start);
				_vCounts.push_back(num);
			}
			void setIndexBuffer(renderHandle h, uint32_t start,
				uint32_t num) override {
				_hasIndex = h.valid();
				_indexBuffer = h.idx;
				_indexFirst = start;
				_indexCount = num;
			}

			void submit(uint8_t view, renderHandle program, uint32_t depth,
				uint16_t /*flags*/) override {
				if (!program.valid()) return;
				drawItem item;
				item.depth = depth;
				item.program = program.idx;
				item.state = _stateBits;
				item.stateColor = _stateColor;
				item.stencilF = _stencilF;
				item.stencilB = _stencilB;
				item.hasModel = _hasModel;
				if (_hasModel) memcpy(item.model, _modelMtx, 64);
				item.hasIndex = _hasIndex;
				item.indexBuffer = _indexBuffer;
				item.indexFirst = _indexFirst;
				item.indexCount = _indexCount;
				item.vBinds = _vBinds;
				item.vStarts = _vStarts;
				item.vCounts = _vCounts;
				for (auto& [uni, tex] : _texUniformBinds)
					item.texBinds.push_back({uni, tex});
				_texUniformBinds.clear();
				auto& draws = ensureView(view).draws;
				draws.push_back(std::move(item));
			}

			// The rest of the draw surface is a documented stub until its
			// pipeline lands (queries, indirect, compute, image access).
			void submitQuery(uint8_t, renderHandle, renderHandle, uint32_t,
				uint16_t) override {}
			void submitIndirect(uint8_t, renderHandle, renderHandle,
				uint16_t, uint16_t, uint32_t, uint16_t) override {}
			void dispatch(uint8_t, renderHandle, uint32_t, uint32_t,
				uint32_t, uint16_t) override {}
			void dispatchIndirect(uint8_t, renderHandle, renderHandle,
				uint16_t, uint16_t, uint16_t) override {}
			void setImage(uint8_t, renderHandle, uint8_t, texAccess,
				texFormat) override {}
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
			void setDebug(bool, bool) override {}

			// ---- framebuffers ----------------------------------------------
			// The descriptor paths the facade's frameBuffer uses: wrapped
			// texture lists, plain sizes, and ratio/nwh (the latter two
			// stay invalid — nothing uses them today).
			renderHandle createFrameBuffer(object config) override {
				const uint16_t idx = _nextFramebuffer++;
				framebufferRecord rec;
				if (config.getType("attachments") == typeList) {
					auto entries = config.getList("attachments");
					for (auto& entry : entries) {
						auto att = entry.getObject();
						uint16_t tex = att.getUInt16("idx",
							uint16_t(0xFFFF));
						auto it = _textures.find(tex);
						if (it == _textures.end()) continue;
						if (sdlFormatIsDepth(it->second.format))
							rec.depth = tex;
						else
							rec.colors.push_back(tex);
					}
				} else if (config.getType("textures") == typeList) {
					auto entries = config.getList("textures");
					for (auto& entry : entries) {
						auto texEntry = entry.getObject();
						uint16_t tex = texEntry.getUInt16("idx",
							uint16_t(0xFFFF));
						auto it = _textures.find(tex);
						if (it == _textures.end()) continue;
						if (sdlFormatIsDepth(it->second.format))
							rec.depth = tex;
						else
							rec.colors.push_back(tex);
					}
				} else {
					auto width = config.getUInt16("width");
					auto height = config.getUInt16("height");
					auto size = config.getVar("size");
					if (size.isVec2()) {
						width = size.getUInt16(0);
						height = size.getUInt16(1);
					}
					if (width == 0 || height == 0) return renderHandle{};
					const auto format = texFormat(config.getUInt16(
						"format", uint16_t(texFormat::RGBA8)));
					SDL_GPUTextureUsageFlags usage =
						SDL_GPU_TEXTUREUSAGE_SAMPLER |
						SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
					auto handle = createTextureWithUsage(
						SDL_GPU_TEXTURETYPE_2D, width, height, 1,
						format, usage);
					if (!handle.valid()) return renderHandle{};
					rec.colors.push_back(handle.idx);
					rec.owned.push_back(handle.idx);
				}
				_framebuffers[idx] = rec;
				return renderHandle{idx};
			}
			renderHandle createFrameBuffer(const void* handles,
				uint8_t num) override {
				const uint16_t idx = _nextFramebuffer++;
				framebufferRecord rec;
				auto textures = (const uint16_t*)handles;
				for (uint8_t i = 0; i < num; ++i) {
					if (!textures[i]) continue;
				rec.colors.push_back(textures[i]);
				}
				_framebuffers[idx] = rec;
				return renderHandle{idx};
			}
			renderHandle createFrameBufferSize(uint16_t w, uint16_t h,
				texFormat f, uint64_t) override {
				SDL_GPUTextureUsageFlags usage =
					SDL_GPU_TEXTUREUSAGE_SAMPLER |
					SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
				auto handle = createTextureWithUsage(
					SDL_GPU_TEXTURETYPE_2D, w, h, 1, f, usage);
				if (!handle.valid()) return renderHandle{};
				framebufferRecord rec;
				rec.owned.push_back(handle.idx);
				const uint16_t idx = _nextFramebuffer++;
				_framebuffers[idx] = rec;
				return renderHandle{idx};
			}
			renderHandle getTexture(renderHandle fb, uint8_t attachment)
				override {
				auto it = _framebuffers.find(fb.idx);
				if (it == _framebuffers.end()) return renderHandle{};
				const size_t a = attachment;
				if (attachment < it->second.colors.size())
					return renderHandle{it->second.colors[a]};
				return renderHandle{};
			}

			// ---- screenshots -------------------------------------------------
			// requestScreenShot: post-submit readback of the frame's
			// target, so the engine's frame-16 aid captures THE RENDER.
			struct pendingShot {
				uint16_t fb;      // 0xFFFF = the back buffer
				string path;
			};
			vector<pendingShot> _shots;

			void requestScreenShot(renderHandle fb, const char* path)
				override {
				_shots.push_back(
					{fb.idx, path ? path : "screenshot"});
			}

			// ---- blit ---------------------------------------------------------
			void blit(uint8_t view, renderHandle dst, uint8_t dstMip,
				uint16_t dstX, uint16_t dstY, uint16_t,
				renderHandle src, uint8_t srcMip, uint16_t srcX,
				uint16_t srcY, uint16_t, uint16_t w, uint16_t h, uint16_t)
				override {
				(void)view;
				auto dIt = _textures.find(dst.idx);
				auto sIt = _textures.find(src.idx);
				if (dIt == _textures.end() || sIt == _textures.end())
					return;
				SDL_GPUCommandBuffer* cmd =
					SDL_AcquireGPUCommandBuffer(_device);
				if (!cmd) return;
				SDL_GPUBlitInfo info {};
				info.source.texture = sIt->second.texture;
				info.source.mip_level = srcMip;
				info.source.x = srcX;
				info.source.y = srcY;
				info.destination.texture = dIt->second.texture;
				info.destination.mip_level = dstMip;
				info.destination.x = dstX;
				info.destination.y = dstY;
				if (w != UINT16_MAX && h != UINT16_MAX) {
					info.source.w = w;
					info.source.h = h;
					info.destination.w = w;
					info.destination.h = h;
				}
				info.load_op = SDL_GPU_LOADOP_DONT_CARE;
				info.filter = SDL_GPU_FILTER_LINEAR;
				SDL_BlitGPUTexture(cmd, &info);
				SDL_SubmitGPUCommandBuffer(cmd);
				SDL_WaitForGPUIdle(_device);
			}

			// ---- render pass pipeline ----------------------------------------
			float alphaRefFloat(uint64_t state) const {
				return float((state >> AlphaRefShift) & 0xFF) / 255.0f;
			}

			void renderViews(SDL_GPUCommandBuffer* cmd,
				SDL_GPUTexture* swapchain, SDL_GPUTextureFormat swapFormat,
				uint32_t swapW, uint32_t swapH) {
				if (_views.size() == 0) return;
				// Ascending view id (bgfx's sort).
				vector<pair<uint8_t, viewRecord*>> order;
				for (auto& [vid, vr] : _views)
					if (vr.touched || !vr.draws.empty())
						order.push_back({vid, &vr});
				std::stable_sort(order.begin(), order.end(),
					[](const auto& a, const auto& b) {
						return a.first < b.first;
					});
				for (auto& [vid, vr] : order) {
					(void)vid;
					// Target: the view's framebuffer, else the swapchain.
					SDL_GPUTexture* colorTex = nullptr;
					SDL_GPUTextureFormat colorFormat;
					SDL_GPUTexture* depthTex = nullptr;
					SDL_GPUTextureFormat depthFormat =
						SDL_GPU_TEXTUREFORMAT_INVALID;
					uint32_t w = 0, h = 0;
					framebufferRecord* fbr = nullptr;
					if (vr->fb != 0xFFFF) {
						auto fbIt = _framebuffers.find(vr->fb);
						if (fbIt != _framebuffers.end())
							fbr = &fbIt->second;
					}
					if (fbr && !fbr->colors.empty()) {
						auto texIt = _textures.find(fbr->colors[0]);
						if (texIt == _textures.end()) continue;
						colorTex = texIt->second.texture;
						colorFormat = texIt->second.format;
						w = texIt->second.width;
						h = texIt->second.height;
						if (fbr->depth != 0xFFFF) {
							auto dIt = _textures.find(fbr->depth);
							if (dIt != _textures.end() &&
								dIt->second.texture) {
								depthTex = dIt->second.texture;
								depthFormat = dIt->second.format;
							}
						}
					} else {
						if (!swapchain) continue;
						colorTex = swapchain;
						colorFormat = swapFormat;
						w = swapW;
						h = swapH;
						// The swapchain has no depth; borrow the per-size
						// D32 texture so depth-tested draws survive.
						if (vr->clearFlags & ClearDepth) {
							depthTex = swapchainDepthTexture(w, h);
							depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
						}
					}
					if (vr->w && vr->h && (vr->w != w || vr->h != h)) {
						w = vr->w;
						h = vr->h;
					}
					if (!w || !h) continue;

					SDL_GPUColorTargetInfo target {};
					target.texture = colorTex;
					target.load_op = (vr->clearFlags & ClearColor)
						? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
					target.store_op = SDL_GPU_STOREOP_STORE;
					target.clear_color.r =
						float((vr->clearColor >> 24) & 0xFF) / 255.0f;
					target.clear_color.g =
						float((vr->clearColor >> 16) & 0xFF) / 255.0f;
					target.clear_color.b =
						float((vr->clearColor >> 8) & 0xFF) / 255.0f;
					target.clear_color.a =
						float(vr->clearColor & 0xFF) / 255.0f;
					SDL_GPUDepthStencilTargetInfo depth {};
					SDL_GPURenderPass* pass = nullptr;
					if (depthTex) {
						depth.texture = depthTex;
						depth.clear_depth = vr->clearDepth;
						depth.load_op = (vr->clearFlags & ClearDepth)
							? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
						depth.stencil_load_op =
							(vr->clearFlags & ClearStencil)
								? SDL_GPU_LOADOP_CLEAR
								: SDL_GPU_LOADOP_LOAD;
						depth.clear_stencil = vr->clearStencil;
						depth.store_op = SDL_GPU_STOREOP_STORE;
						depth.stencil_store_op = SDL_GPU_STOREOP_STORE;
						pass = SDL_BeginGPURenderPass(cmd, &target, 1,
							&depth);
					} else {
						pass = SDL_BeginGPURenderPass(cmd, &target, 1,
							nullptr);
					}
					if (!pass) continue;

					if (vr->x || vr->y || w != vr->w || h != vr->h) {
						SDL_GPUViewport vp {};
						vp.x = float(vr->x);
						vp.y = float(vr->y);
						vp.w = float(vr->w ? vr->w : w);
						vp.h = float(vr->h ? vr->h : h);
						vp.min_depth = 0.0f;
						vp.max_depth = 1.0f;
						SDL_SetGPUViewport(pass, &vp);
					}
					for (auto& item : vr->draws)
						drawItemToPass(cmd, pass, vr, item, colorFormat,
							depthFormat);
					SDL_EndGPURenderPass(pass);
				}
			}

			SDL_GPUTexture* swapchainDepthTexture(uint32_t w, uint32_t h) {
				auto it = _depthTextures.find({w, h});
				if (it != _depthTextures.end()) return it->second;
				SDL_GPUTextureCreateInfo ci {};
				ci.type = SDL_GPU_TEXTURETYPE_2D;
				ci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
				ci.width = w;
				ci.height = h;
				ci.layer_count_or_depth = 1;
				ci.num_levels = 1;
				ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
				SDL_GPUTexture* tex = SDL_CreateGPUTexture(_device, &ci);
				if (!tex) {
					fprintf(stderr, "[sdlgpu] %s\n", SDL_GetError());
					return nullptr;
				}
				_depthTextures[{w, h}] = tex;
				return tex;
			}

			// The draw: pipeline (cached), the uniform packets, the binds.
			void drawItemToPass(SDL_GPUCommandBuffer* cmd,
				SDL_GPURenderPass* pass, viewRecord* vr, drawItem& item,
				SDL_GPUTextureFormat colorFormat,
				SDL_GPUTextureFormat depthFormat) {
				// A draw with no vertex buffers: only a clear matters.
				if (item.vBinds.empty() && !item.hasIndex) return;
				auto progIt = _programs.find(item.program);
				if (progIt == _programs.end()) return;
				auto pipeline = getPipeline(progIt->second, item,
					colorFormat, depthFormat);
				SDL_BindGPUGraphicsPipeline(pass, pipeline);

				pushUniformPackets(cmd, progIt->second, vr, item);

				// Samplers: SDL requires every declared sampler bound, and
				// slots contiguous from zero (the bind array's index IS the
				// slot). The item's binds resolve uniform NAME -> the
				// fs's (stripped) order; unbound slots ride the filler.
				auto fsProgramIt = _programs.find(item.program);
				if (fsProgramIt != _programs.end()) {
					auto fsRecIt = _stages.find(
						fsProgramIt->second.fs);
					if (fsRecIt != _stages.end()
						&& !fsRecIt->second.samplers.empty()) {
						auto& fsRec = fsRecIt->second;
						vector<SDL_GPUTextureSamplerBinding> binds(
							fsRec.samplers.size());
						for (uint32_t s = 0; s < binds.size(); ++s) {
							binds[s].sampler = fillerSampler();
							binds[s].texture = fillerTexture();
						}
						for (auto& [uni, tex] : item.texBinds) {
							auto uIt = _uniforms.find(uni);
							if (uIt == _uniforms.end()) continue;
							auto slot = fsRec.samplerSlots.find(
								uIt->second.name);
							if (slot == fsRec.samplerSlots.end())
								continue;
							auto texIt = _textures.find(tex);
							if (texIt == _textures.end()) continue;
							binds[slot->second].sampler =
								samplerFor(texIt->second);
							binds[slot->second].texture =
								texIt->second.texture;
						}
						SDL_BindGPUFragmentSamplers(pass, 0,
							binds.data(), uint32_t(binds.size()));
					}
				}

				vector<SDL_GPUBufferBinding> bindings;
				for (auto& [slot, buf] : item.vBinds) {
					auto it = _buffers.find(buf);
					if (it == _buffers.end()) continue;
					SDL_GPUBufferBinding b {};
					b.buffer = it->second.buffer;
					b.offset = 0;
					bindings.push_back(b);
				}
				if (!bindings.empty())
					SDL_BindGPUVertexBuffers(pass, 0, bindings.data(),
						uint32_t(item.vBinds.size()));
				if (item.hasIndex) {
					auto it = _buffers.find(item.indexBuffer);
					if (it != _buffers.end()) {
						SDL_GPUBufferBinding ib {};
						ib.buffer = it->second.buffer;
						SDL_BindGPUIndexBuffer(pass, &ib,
							it->second.index32
								? SDL_GPU_INDEXELEMENTSIZE_32BIT
								: SDL_GPU_INDEXELEMENTSIZE_16BIT);
						const uint32_t count = item.indexCount
							? item.indexCount
							: (it->second.size
								  / (it->second.index32 ? 4u : 2u))
								  - item.indexFirst;
						SDL_DrawGPUIndexedPrimitives(pass, count, 1,
							item.indexFirst, 0, 0);
						return;
					}
				}
				// No index buffer: vertex-only draw.
				auto vIt = _buffers.find(item.vBinds[0].second);
				if (vIt == _buffers.end()) return;
				const uint32_t stride =
					max<uint32_t>(1, vIt->second.layout.stride);
				const uint32_t count = item.vCounts[0]
					? item.vCounts[0]
					: vIt->second.size / stride;
				SDL_DrawGPUPrimitives(pass, count, 1,
					item.vStarts[0], 0);
			}

			void pushUniformPackets(SDL_GPUCommandBuffer* cmd,
				programRecord& prog, viewRecord* vr, drawItem& item) {
				const float* view = vr->hasTransform ? vr->view : nullptr;
				const float* proj = vr->hasTransform ? vr->proj : nullptr;
				pushStagePacket(cmd, prog.vs, vr, view, proj, item);
				pushStagePacket(cmd, prog.fs, vr, view, proj, item);
			}

			// bgfx's predefined uniform names, as the compiled shaders
			// declare them.
			static bool isPredefine(const string& name,
				uint32_t& slotId) {
				static const char* names[] = {
					"u_viewRect",     // 0
					"u_viewTexel",    // 1
					"u_view",         // 2
					"u_invView",      // 3
					"u_proj",         // 4
					"u_invProj",      // 5
					"u_viewProj",     // 6
					"u_invViewProj",  // 7
					"u_model",        // 8
					"u_modelView",    // 9
					"u_invModelView", // 10
					"u_modelViewProj",// 11
					"u_alphaRef4",    // 12
				};
				for (uint32_t i = 0;
						i < sizeof(names) / sizeof(names[0]); ++i)
					if (names[i] == name) {
						slotId = i;
						return true;
					}
				return false;
			}

			void writeMatrix(vector<uint8_t>& packet, uint32_t offset,
				const float* mtx) {
				if (offset + 64 > packet.size()) return;
				memcpy(packet.data() + offset, mtx, 64);
			}

			// Column-A*B in row-major byte form: result[r][c] =
			// sum_k r[r][k] * r2[k][c]. Matches bgfx's float4x4_mul
			// byte layout (the non-SIMD column-major multiply).
			static void rowMajorMul(float* out, const float* a,
				const float* b) {
				bx::float4x4_t am, bm, rm;
				memcpy(&am, a, 64);
				memcpy(&bm, b, 64);
				bx::float4x4_mul(&rm, &am, &bm);
				memcpy(out, &rm, 64);
			}

			void pushStagePacket(SDL_GPUCommandBuffer* cmd, uint16_t stage,
				viewRecord* vr, const float* view, const float* proj,
				const drawItem& item) {
				if (stage == 0xFFFF) return;
				auto it = _stages.find(stage);
				if (it == _stages.end()) return;
				auto& rec = it->second;
				if (!rec.blockSize) return;
				// The whole block, zero-filled: unset uniforms read
				// zeroes the way bgfx's fresh uniform buffers do.
				vector<uint8_t> packet(rec.blockSize, uint8_t(0));

				// Derived matrices, built on demand.
				const float* model = item.hasModel ? item.model
												   : identity4();
				float viewProj[16] = {0};
				float modelView[16] = {0};
				float modelViewProj[16] = {0};
				if (view && proj)
					rowMajorMul(viewProj, view, proj);
				// (bx's mul takes the matrices as-is: the byte layout
				// is the same the GL driver reads.)

				for (auto& entry : rec.uniforms) {
					const uint32_t span = uint32_t(entry.slots) * 16u;
					if (entry.offset + span > packet.size()) continue;
					uint32_t slotId;
					if (isPredefine(entry.name, slotId)) {
						const float* mtx = nullptr;
						if (view && proj)
							switch (slotId) {
							case 2: /* u_view */
								mtx = view;
								break;
							case 4: /* u_proj */
								mtx = proj;
								break;
							case 6: /* u_viewProj */
								mtx = viewProj;
								break;
							default:
								break;
							}
						switch (slotId) {
						case 0: {  // u_viewRect
							float rect[4] = {
								float(vr->x), float(vr->y),
								float(vr->w ? vr->w : 1),
								float(vr->h ? vr->h : 1)};
							memcpy(packet.data() + entry.offset, rect, 16);
							continue;
						}
						case 1: {  // u_viewTexel
							float texel[4] = {
								1.0f / float(vr->w ? vr->w : 1),
								1.0f / float(vr->h ? vr->h : 1),
								0.0f, 1.0f};
							memcpy(packet.data() + entry.offset, texel, 16);
							continue;
						}
						case 3: /* u_invView */
						case 5: /* u_invProj */
						case 7: /* u_invViewProj */ {
							if (!view || !proj) {
								memset(packet.data() + entry.offset, 0,
									span);
								continue;
							}
							bx::float4x4_t src;
							const float* base = slotId == 3
								? view
								: slotId == 5 ? proj : viewProj;
							memcpy(&src, base, 64);
							bx::float4x4_t inv;
							bx::float4x4_inverse(&inv, &src);
							writeMatrix(packet, entry.offset,
								reinterpret_cast<const float*>(&inv));
							continue;
						}
						case 8: /* u_model */
							writeMatrix(packet, entry.offset, model);
							continue;
						case 9: /* u_modelView */ {
							if (!view) {
								memset(packet.data() + entry.offset, 0,
									span);
								continue;
							}
							rowMajorMul(modelView, model, view);
							writeMatrix(packet, entry.offset, modelView);
							continue;
						}
						case 10: /* u_invModelView */ {
							if (!view) continue;
							bx::float4x4_t mv, inv;
							rowMajorMul(modelView, model, view);
							memcpy(&mv, modelView, 64);
							bx::float4x4_inverse(&inv, &mv);
							writeMatrix(packet, entry.offset,
								reinterpret_cast<const float*>(&inv));
							continue;
						}
						case 11: /* u_modelViewProj */ {
							rowMajorMul(modelViewProj, model,
								viewProj);
							writeMatrix(packet, entry.offset,
								modelViewProj);
							continue;
						}
						case 12: {  // u_alphaRef4
							float a[4] = {alphaRefFloat(item.state),
								0.0f, 0.0f, 0.0f};
							memcpy(packet.data() + entry.offset, a, 16);
							continue;
						}
						}
						(void)mtx;
						continue;
					}
					// A user uniform: copy the shadow by name.
					for (auto& [_, urec] : _uniforms) {
						if (urec.name != entry.name) continue;
						const uint32_t srcSize =
							uint32_t(urec.shadow.size());
						uint32_t toCopy = min(srcSize, span);
						if (entry.type == renderUniformType::Mat3) {
							// Gold's 9-float rows pad into the block's
							// vec4 rows ([row, 0] each — the transpose
							// duality the GL path shows).
							const auto* src =
								(const float*)urec.shadow.data();
							for (uint16_t item2 = 0; item2 < entry.num;
								 ++item2) {
								for (uint8_t row = 0; row < 3; ++row) {
									float rowVec[4] = {0, 0, 0, 0};
									const uint32_t base =
										item2 * 9 + row * 3;
									if (base + 3 <= srcSize / 4)
										memcpy(rowVec,
											src + base, 12);
									memcpy(
										packet.data() + entry.offset
											+ (item2 * 3 + row) * 16,
										rowVec, 16);
								}
							}
							break;
						}
						memcpy(packet.data() + entry.offset,
							urec.shadow.data(), toCopy);
						break;
					}
				}

				if (rec.fragment)
					SDL_PushGPUFragmentUniformData(cmd, 0, packet.data(),
						uint32_t(packet.size()));
				else
					SDL_PushGPUVertexUniformData(cmd, 0, packet.data(),
						uint32_t(packet.size()));
			}

			static const float* identity4() {
				static const float identity[16] = {
					1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					0, 0, 0, 1,
				};
				return identity;
			}

			// ---- pipelines ----------------------------------------------------
			// The gold draw-state bits become SDL's pipeline. The cache
			// keys on (program, state, stencils, target formats, layout).
			SDL_GPUGraphicsPipeline* getPipeline(programRecord& prog,
				drawItem& item, SDL_GPUTextureFormat colorFormat,
				SDL_GPUTextureFormat depthFormat) {
				uint64_t layoutHash = 0;
				if (!item.vBinds.empty()) {
					auto it = _buffers.find(item.vBinds[0].second);
					if (it != _buffers.end()) {
						const auto& lay = it->second.layout;
						for (auto& a : lay.attribs)
							layoutHash = layoutHash * 31 + a.id * 7
								+ hash<uint32_t>()(a.offset) * 13
								+ hash<uint32_t>()(uint32_t(a.format))
								* 17;
						layoutHash =
							layoutHash * 31 + lay.stride;
					}
				}
				pipelineKey key {item.program, item.state, item.stencilF,
					item.stencilB, colorFormat, depthFormat};
				key.state ^= layoutHash;  // fold the layout in
				auto it = _pipelines.find(key);
				if (it != _pipelines.end()) return it->second;

				const auto state = item.state;
				SDL_GPUGraphicsPipelineCreateInfo ci {};
				auto vsStage = _stages.find(prog.vs);
				auto fsStage = _stages.find(prog.fs);
				if (vsStage != _stages.end())
					ci.vertex_shader = vsStage->second.shader;
				if (fsStage != _stages.end())
					ci.fragment_shader = fsStage->second.shader;

				// Vertex input: the buffer's layout descriptors mapped
				// through the VS's io locations.
				if (!item.vBinds.empty()) {
					auto vIt = _buffers.find(item.vBinds[0].second);
					if (vIt != _buffers.end()) {
						const auto& lay = vIt->second.layout;
						const auto& ioLocs = vsStage != _stages.end()
							? vsStage->second.ioLocations
							: map<string, uint32_t>();
						SDL_GPUVertexBufferDescription desc[1];
						SDL_GPUVertexAttribute attrs[18];
						uint32_t attrCount = 0;
						for (auto& a : lay.attribs) {
							const auto loc = ioLocs.find(
								attribNameFor(a.id));
							if (loc == ioLocs.end()) continue;
							attrs[attrCount].location =
								uint32_t(loc->second);
							attrs[attrCount].buffer_slot = 0;
							attrs[attrCount].format = a.format;
							attrs[attrCount].offset = a.offset;
							++attrCount;
						}
						desc[0].slot = 0;
						desc[0].pitch = max<uint32_t>(1, lay.stride);
						desc[0].input_rate =
							SDL_GPU_VERTEXINPUTRATE_VERTEX;
						desc[0].instance_step_rate = 0;
						ci.vertex_input_state.num_vertex_buffers = 1;
						ci.vertex_input_state.vertex_buffer_descriptions
							= desc;
						ci.vertex_input_state.num_vertex_attributes =
							attrCount;
						ci.vertex_input_state.vertex_attributes = attrs;
					}
				}

				// Primitive type.
				const auto prim = (state >> PrimitiveShift) & 0x7;
				switch (prim) {
				case uint64_t(PrimitiveTriStrip):
					ci.primitive_type =
						SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
					break;
				case uint64_t(PrimitiveLines):
					ci.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
					break;
				case uint64_t(PrimitiveLineStrip):
					ci.primitive_type =
						SDL_GPU_PRIMITIVETYPE_LINESTRIP;
					break;
				case uint64_t(PrimitivePoints):
					ci.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST;
					break;
				default:
					ci.primitive_type =
						SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
					break;
				}

				// Rasterizer: bgfx's front face convention (CW default),
				// the cull on the gold field.
				auto& raster = ci.rasterizer_state;
				raster.fill_mode = SDL_GPU_FILLMODE_FILL;
				raster.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
				const auto cull = (state >> CullShift) & 0x3;
				raster.cull_mode = cull == uint64_t(CullCW)
					? SDL_GPU_CULLMODE_FRONT
					: cull == uint64_t(CullCCW)
						? SDL_GPU_CULLMODE_BACK
						: SDL_GPU_CULLMODE_NONE;
				raster.enable_depth_clip = true;

				// Depth.
				auto& ds = ci.depth_stencil_state;
				const auto compare = (state >> DepthCompareShift) & 0x7;
				ds.compare_op = sdlCompareOp(compare);
				ds.enable_depth_test = compare != uint64_t(DepthAlways)
					&& depthFormat
						!= SDL_GPU_TEXTUREFORMAT_INVALID;
				ds.enable_depth_write = (state & WriteZ) != 0
					&& depthFormat != SDL_GPU_TEXTUREFORMAT_INVALID;

				// Stencil (rarely used; the parser's zeros mean "off").
				ds.front_stencil_state = stencilStateFromGold(
					item.stencilF);
				ds.back_stencil_state = stencilStateFromGold(
					item.stencilB);
				// The parser's zero stencil = test-always + keep ops (off).
				ds.enable_stencil_test = item.stencilF != 0
					|| item.stencilB != 0;

				// Color target + blend.
				auto& target = ci.target_info;
				static SDL_GPUColorTargetDescription targets[1];
				targets[0].format = colorFormat;
				auto& blend = targets[0].blend_state;
				const auto mask = uint32_t(
					((state & WriteR) ? 1u : 0u)
					| ((state & WriteG) ? 2u : 0u)
					| ((state & WriteB) ? 4u : 0u)
					| ((state & WriteA) ? 8u : 0u));
				blend.color_write_mask = mask;
				blend.enable_color_write_mask = true;
				if (state & BlendEnabled) {
					blend.enable_blend = true;
					blend.src_color_blendfactor = sdlBlendFactor(
						(state >> BlendRGBSrcShift) & 0xF);
					blend.dst_color_blendfactor = sdlBlendFactor(
						(state >> BlendRGBDstShift) & 0xF);
					blend.src_alpha_blendfactor = sdlBlendFactor(
						(state >> BlendASrcShift) & 0xF);
					blend.dst_alpha_blendfactor = sdlBlendFactor(
						(state >> BlendADstShift) & 0xF);
					const auto op = (state >> BlendEquationShift) & 0x7;
					blend.color_blend_op = sdlBlendOp(op);
					blend.alpha_blend_op = sdlBlendOp(op);
				}
				target.num_color_targets = 1;
				target.color_target_descriptions = targets;
				target.depth_stencil_format = depthFormat;
				target.has_depth_stencil_target = depthFormat
					!= SDL_GPU_TEXTUREFORMAT_INVALID;

				ci.multisample_state.sample_count =
					SDL_GPU_SAMPLECOUNT_1;

				SDL_GPUGraphicsPipeline* pipeline =
					SDL_CreateGPUGraphicsPipeline(_device, &ci);
				if (!pipeline) {
					fprintf(stderr, "[sdlgpu] pipeline: %s\n",
						SDL_GetError());
					return nullptr;
				}
				_pipelines[key] = pipeline;
				return pipeline;
			}

			// gold id -> SDL tables.
			static SDL_GPUCompareOp sdlCompareOp(uint64_t gold) {
				switch (gold & 0x7) {
				case uint64_t(DepthAlways):
					return SDL_GPU_COMPAREOP_ALWAYS;
				case uint64_t(DepthLess):
					return SDL_GPU_COMPAREOP_LESS;
				case uint64_t(DepthLEqual):
					return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
				case uint64_t(DepthEqual):
					return SDL_GPU_COMPAREOP_EQUAL;
				case uint64_t(DepthGEqual):
					return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
				case uint64_t(DepthGreater):
					return SDL_GPU_COMPAREOP_GREATER;
				case uint64_t(DepthNotEqual):
					return SDL_GPU_COMPAREOP_NOT_EQUAL;
				default:
					return SDL_GPU_COMPAREOP_NEVER;
				}
			}
			static SDL_GPUStencilOpState stencilStateFromGold(
				uint32_t gold) {
				SDL_GPUStencilOpState st {};
				st.compare_op = sdlCompareOp((gold >> 0) & 0x7);
				st.fail_op = sdlStencilOp((gold >> 3) & 0x7);
				st.depth_fail_op = sdlStencilOp((gold >> 6) & 0x7);
				st.pass_op = sdlStencilOp((gold >> 9) & 0x7);
				return st;
			}
			static SDL_GPUStencilOp sdlStencilOp(uint32_t gold) {
				switch (gold & 0x7) {
				case uint32_t(OpKeep): return SDL_GPU_STENCILOP_KEEP;
				case uint32_t(OpZero): return SDL_GPU_STENCILOP_ZERO;
				case uint32_t(OpReplace):
					return SDL_GPU_STENCILOP_REPLACE;
				case uint32_t(OpIncr):
					return SDL_GPU_STENCILOP_INCREMENT_AND_WRAP;
				case uint32_t(OpIncrSat):
					return SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP;
				case uint32_t(OpDecr):
					return SDL_GPU_STENCILOP_DECREMENT_AND_WRAP;
				case uint32_t(OpDecrSat):
					return SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP;
				default: return SDL_GPU_STENCILOP_INVERT;
				}
			}
			static SDL_GPUBlendFactor sdlBlendFactor(uint64_t gold) {
				switch (gold & 0xF) {
				case uint64_t(FactorOne):
					return SDL_GPU_BLENDFACTOR_ONE;
				case uint64_t(FactorSrcColor):
					return SDL_GPU_BLENDFACTOR_SRC_COLOR;
				case uint64_t(FactorInvSrcColor):
					return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
				case uint64_t(FactorSrcAlpha):
					return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				case uint64_t(FactorInvSrcAlpha):
					return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				case uint64_t(FactorDstAlpha):
					return SDL_GPU_BLENDFACTOR_DST_ALPHA;
				case uint64_t(FactorInvDstAlpha):
					return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
				case uint64_t(FactorDstColor):
					return SDL_GPU_BLENDFACTOR_DST_COLOR;
				case uint64_t(FactorInvDstColor):
					return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
				case uint64_t(FactorSrcAlphaSat):
					return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
				case uint64_t(FactorBlendFactor):
					return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
				case uint64_t(FactorInvBlendFactor):
					return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
				default:
					return SDL_GPU_BLENDFACTOR_ZERO;
				}
			}
			static SDL_GPUBlendOp sdlBlendOp(uint64_t gold) {
				switch (gold & 0x7) {
				case uint64_t(BlendSub):
					return SDL_GPU_BLENDOP_SUBTRACT;
				case uint64_t(BlendRevSub):
					return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
				case uint64_t(BlendMin):
					return SDL_GPU_BLENDOP_MIN;
				case uint64_t(BlendMax):
					return SDL_GPU_BLENDOP_MAX;
				default:
					return SDL_GPU_BLENDOP_ADD;
				}
			}

			static const char* attribNameFor(uint8_t id) {
				switch (id) {
				case 0: return "a_position";
				case 1: return "a_normal";
				case 2: return "a_tangent";
				case 3: return "a_bitangent";
				case 4: return "a_color0";
				case 5: return "a_color1";
				case 6: return "a_color2";
				case 7: return "a_color3";
				case 8: return "a_indices";
				case 9: return "a_weight";
				case 10: return "a_texcoord0";
				case 11: return "a_texcoord1";
				case 12: return "a_texcoord2";
				case 13: return "a_texcoord3";
				case 14: return "a_texcoord4";
				case 15: return "a_texcoord5";
				case 16: return "a_texcoord6";
				case 17: return "a_texcoord7";
				default: return "";
				}
			}

			// A 1x1 white filler for the sampler slots that carry no
			// texture (SDL requires every declared sampler bound); its
			// pixels ride the next copy pass.
			SDL_GPUTexture* _fillerTex = nullptr;
			SDL_GPUSampler* _fillerSmp = nullptr;
			bool _fillerUploadPending = false;

			SDL_GPUTexture* fillerTexture() {
				if (_fillerTex) return _fillerTex;
				SDL_GPUTextureCreateInfo ci {};
				ci.type = SDL_GPU_TEXTURETYPE_2D;
				ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				ci.width = 1;
				ci.height = 1;
				ci.layer_count_or_depth = 1;
				ci.num_levels = 1;
				ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
				SDL_GPUTexture* tex = SDL_CreateGPUTexture(_device, &ci);
				SDL_GPUSamplerCreateInfo si {};
				si.min_filter = SDL_GPU_FILTER_NEAREST;
				si.mag_filter = SDL_GPU_FILTER_NEAREST;
				SDL_GPUSampler* sampler = SDL_CreateGPUSampler(_device, &si);
				if (!tex || !sampler) {
					fprintf(stderr, "[sdlgpu] filler: %s\n",
						SDL_GetError());
					SDL_ReleaseGPUTexture(_device, tex);
					SDL_ReleaseGPUSampler(_device, sampler);
					return nullptr;
				}
				_fillerTex = tex;
				_fillerSmp = sampler;
				_fillerUploadPending = true;
				return _fillerTex;
			}

			SDL_GPUSampler* fillerSampler() {
				fillerTexture();
				return _fillerSmp;
			}

			// The texture's sampler: the gold sampler flag word -> SDL.
			SDL_GPUSampler* samplerFor(textureRecord& rec) {
				if (rec.sampler) return rec.sampler;
				SDL_GPUSamplerCreateInfo si {};
				auto gold = rec.samplerFlags;
				auto wrap = [&](uint32_t mode) {
					if (mode & 1)
						return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
					if (mode & 2)
						return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
					if (mode & 4)
						return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
					return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
				};
				si.address_mode_u = wrap(gold & 0x7);
				si.address_mode_v = wrap((gold >> 3) & 0x7);
				si.address_mode_w = wrap((gold >> 6) & 0x7);
				si.min_filter = (gold & MinPoint)
					? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
				si.mag_filter = (gold & MagPoint)
					? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
				si.mipmap_mode = (gold & MipPoint)
					? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
					: SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
				si.enable_anisotropy =
					(gold & (MinAnisotropic | MagAnisotropic)) != 0;
				si.max_anisotropy = 16.0f;
				if (gold & CompareEnabled) {
					si.enable_compare = true;
					si.compare_op = sdlCompareOp(
						(gold >> CompareModeShift) & 0x7);
				}
				rec.sampler = SDL_CreateGPUSampler(_device, &si);
				return rec.sampler;
			}

			static bool sdlFormatIsDepth(SDL_GPUTextureFormat format) {
				switch (format) {
				case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
				case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
				case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
				case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
				case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
					return true;
				default:
					return false;
				}
			}

			// createTextureLike's RT-capable variant.
			renderHandle createTextureWithUsage(SDL_GPUTextureType type,
				uint32_t w, uint32_t h, uint32_t d,
				texFormat f, SDL_GPUTextureUsageFlags usage) {
				(void)d;
				if (w == 0 || h == 0) {
					fprintf(stderr,
						"[sdlgpu] zero-dim RT create: %ux%u (type %d, "
						"fb-path)\n", w, h, int(type));
					return renderHandle{};
				}
				const bool depth =
					SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET & usage;
				auto format = depth
					? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : toSDLFormat(f,
						false);
				SDL_GPUTextureCreateInfo ci {};
				ci.type = type;
				ci.format = format;
				ci.width = w;
				ci.height = h;
				ci.layer_count_or_depth = 1;
				ci.num_levels = 1;
				ci.usage = usage;
				SDL_GPUTexture* tex = SDL_CreateGPUTexture(_device, &ci);
				if (!tex) return renderHandle{};
				const uint16_t idx = _nextTexture++;
				textureRecord rec;
				rec.texture = tex;
				rec.format = format;
				rec.type = type;
				rec.width = w;
				rec.height = h;
				rec.depth = 1;
				rec.numMips = 1;
				rec.numLayers = 1;
				_textures[idx] = rec;
				return renderHandle{idx};
			}

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