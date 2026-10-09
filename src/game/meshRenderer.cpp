#include "meshRenderer.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/timer.h>

#include <cstring>
#include <fstream>
#include <set>

#include "entity.hpp"
#include "envMap.hpp"
#include "graphics.hpp"
#include "light.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include "transform.hpp"

namespace gold {
	using namespace std;
	using namespace bgfx;

	struct pbrUniformSet {
		float u_MipCount;
		float u_OcclusionStrength;
		float u_NormalScale;
		float u_Exposure;

		float u_SheenRoughness;
		float u_Anisotropy;
		float u_SubsurfaceScale;
		float u_SubsurfaceDistortion;

		float u_SubsurfacePower;
		float u_ThinFilmThicknessMinimum;
		float u_ThinFilmThicknessMaximum;
		float u_Thickness;

		float u_NormalUVSet;
		float u_EmissiveUVSet;
		float u_OcclusionUVSet;
		float u_BaseColorUVSet;

		float u_MetallicRoughnessUVSet;
		float u_DiffuseUVSet;
		float u_SpecularGlossinessUVSet;
		float u_ClearcoatUVSet;

		float u_ClearcoatRoughnessUVSet;
		float u_ClearcoatNormalUVSet;
		float u_SheenColorIntensityUVSet;
		float u_MetallicRougnessSpecularTextureUVSet;

		float u_SubsurfaceColorUVSet;
		float u_SubsurfaceThicknessUVSet;
		float u_ThinFilmUVSet;
		float u_ThinFilmThicknessUVSet;

		float u_ThicknessUVSet;
		float u_AnisotropyUVSet;
		float u_AnisotropyDirectionUVSet;
		float u_MetallicFactor;

		float u_RoughnessFactor;
		float u_GlossinessFactor;
		float u_SheenIntensityFactor;
		float u_ClearcoatFactor;

		float u_ClearcoatRoughnessFactor;
		float u_MetallicRoughnessSpecularFactor;
		float u_SubsurfaceThicknessFactor;
		float u_ThinFilmFactor;

		float u_Transmission;
		float u_AlphaCutoff;
		float u_IOR_and_f0[2];

		float u_BaseColorFactor[4];
		float u_DiffuseFactor[4];
		float u_SpecularFactor[4];
		float u_SheenColorFactor[4];
		float u_AnisotropyDirection[4];
		float u_SubsurfaceColorFactor[4];
		float u_AbsorptionColor[4];
		float u_Camera[4];
		float u_EmissiveFactor[4];
	};

	object& meshRenderer::getPrototype() {
		static auto proto = object{
			{"priority", priorityEnum::drawPriority},
			// Its C++ methods bind on the prototype like every other
			// component class; without them initComps dispatched nothing
			// and the draw path never got a program.
			{"initialize", method(&meshRenderer::initialize)},
			{"draw", method(&meshRenderer::draw)},
			{"node", ""},
			{"mesh", var()},
			{"proto", renderable::getPrototype()},
		};
		return proto;
	}

	void meshRenderer::setMaterial(
		camera cam, object primitive, object) {
		auto meshD = getObject<mesh>("mesh");
		auto materials = meshD.getList("materials");
		auto matId = primitive.getUInt64("material", UINT64_MAX);
		auto mat = materials.getObject(matId);
		if (!mat) return;
		auto program = primitive.getObject<shaderProgram>("program");
		// Sampler stages follow uniforms.sh's wiring exactly; the
		// texture library's getXxxUV functions and samplers pair on
		// those stage numbers.
		auto bindTexture = [&](object info, string sampler, uint8_t stage) {
			if (!info || !program) return;
			auto resolved = info.getObject("resolvedTexture");
			auto image = resolved.getObject("image");
			if (!image) return;
			auto data = image.getBinary("data");
			if (data.empty()) return;
			// getObject's default for a missing key is gpuTexture(),
			// whose ctor sets a parent and thus reads as TRUTHY (with an
			// idx inherited from the prototype) — creation was silently
			// skipped, forever binding stage N to nothing.
			if (info.getType("gpuTexture") != typeObject) {
				info.setObject(
					"gpuTexture",
					gpuTexture(obj({
						{"data", data},
						{"name", sampler + ":" + mat.getString("name")},
					})));
			}
			auto tex = info.getObject<gpuTexture>("gpuTexture");
			shaderProgram::bindTexture(sampler, stage, tex);
		};
		auto set = pbrUniformSet();
		memset(&set, 0, sizeof(pbrUniformSet));
		set.u_BaseColorFactor[0] = 1.0f;
		set.u_BaseColorFactor[1] = 1.0f;
		set.u_BaseColorFactor[2] = 1.0f;
		set.u_BaseColorFactor[3] = 1.0f;
		set.u_MetallicFactor = 1.0f;
		set.u_RoughnessFactor = 1.0f;
		set.u_NormalScale = 1.0f;
		set.u_OcclusionStrength = 1.0f;
		// toneMap() multiplies the final color by u_Exposure; the
		// zeroed default would blank the frame.
		set.u_Exposure = 1.0f;
		// The camera's world position feeds the specular/reflection
		// paths (u_Camera = slot 18).
		{
			auto camTrans = cam.getTransform();
			auto camMtx = camTrans.getWorldMatrix();
			auto eye = camMtx * vec4f(0, 0, 0, 1);
			set.u_Camera[0] = eye.getFloat(0);
			set.u_Camera[1] = eye.getFloat(1);
			set.u_Camera[2] = eye.getFloat(2);
		}
		auto uploadFactor = [&](object owner, string key, float* target,
				uint8_t count) {
			auto value = owner.getVar(key);
			if (value.isList()) {
				auto values = value.getList();
				for (uint8_t i = 0; i < count && i < values.size(); ++i)
					target[i] = values.getFloat(i);
			} else if (value.isVec2() || value.isVec3() || value.isVec4()) {
				for (uint8_t i = 0; i < count; ++i)
					target[i] = value.getFloat(i);
			}
		};
		auto uploadScalar = [&](object owner, string key, float& target) {
			auto value = owner.getVar(key);
			if (value.isNumber()) target = value.getFloat();
		};
		auto clCoTex = mat.getObject("clearcoatTexture");
		auto subCoTex = mat.getObject("subsurfaceColorTexture");
		auto subThTex = mat.getObject("subsurfaceThicknessTexture");
		auto clRoTex = mat.getObject("clearcoatRoughnessTexture");
		auto clNoTex = mat.getObject("clearcoatNormalTexture");
		auto coInTex = mat.getObject("colorIntensityTexture");
		auto metRoSpecTex =
			mat.getObject("metallicRoughnessSpecularTexture");
		auto anisTex = mat.getObject("anisotropyTexture");
		auto dirTex = mat.getObject("anisotropyDirectionTexture");
		auto filmThinTex = mat.getObject("thinfilmTexture");
		auto filmThickTex = mat.getObject("thinfilmThicknessTexture");
		auto thickTex = mat.getObject("thicknessTexture");
		auto difTex = mat.getObject("diffuseTexture");
		auto specTex = mat.getObject("specularGlossinessTexture");
		auto occTex = mat.getObject("occlusionTexture");
		auto normTex = mat.getObject("normalTexture");
		auto emTex = mat.getObject("emissiveTexture");
		auto pbrMetRo = mat.getObject("pbrMetallicRoughness");
		if (pbrMetRo) {
			uploadFactor(pbrMetRo, "baseColorFactor", set.u_BaseColorFactor, 4);
			uploadScalar(pbrMetRo, "metallicFactor", set.u_MetallicFactor);
			uploadScalar(pbrMetRo, "roughnessFactor", set.u_RoughnessFactor);
			auto base = pbrMetRo.getObject("baseColorTexture");
			auto metTex =
				pbrMetRo.getObject("metallicRoughnessTexture");
			bindTexture(base, "u_BaseColorSampler", 3);
			bindTexture(metTex, "u_MetallicRoughnessSampler", 4);
		}
		uploadFactor(mat, "emissiveFactor", set.u_EmissiveFactor, 3);
		if (normTex) uploadScalar(normTex, "scale", set.u_NormalScale);
		if (occTex) uploadScalar(occTex, "strength", set.u_OcclusionStrength);
		if (program) {
			// The PBR uniform bank: 20 vec4 slots uploaded one by one —
			// on this bgfx fork, per-draw array uniforms (u_UniformSet[20])
			// arrive as zeros while individually-named vec4s land fine.
			static const char* bankNames[5] = {
				"u_PBRBank0", "u_PBRBank1", "u_PBRBank2", "u_PBRBank3",
				"u_PBRBank4",
			};
			const float* slots = (const float*)&set;
			for (uint8_t i = 0; i < 5; ++i)
				shaderProgram::setUniform(bankNames[i], slots + i * 16);
		}
		bindTexture(normTex, "u_NormalSampler", 0);
		bindTexture(emTex, "u_EmissiveSampler", 1);
		bindTexture(occTex, "u_OcclusionSampler", 2);
		bindTexture(difTex, "u_DiffuseSampler", 5);
		bindTexture(specTex, "u_SpecularGlossinessSampler", 6);
		bindTexture(clCoTex, "u_ClearcoatSampler", 9);
		bindTexture(clRoTex, "u_ClearcoatRoughnessSampler", 10);
		bindTexture(clNoTex, "u_ClearcoatNormalSampler", 11);
		bindTexture(coInTex, "u_SheenColorIntensitySampler", 12);
		bindTexture(metRoSpecTex, "u_MetallicRoughnessSpecularSampler", 13);
		bindTexture(subCoTex, "u_SubsurfaceColorSampler", 14);
		bindTexture(subThTex, "u_SubsurfaceThicknessSampler", 15);
		bindTexture(filmThinTex, "u_ThinFilmSampler", 17);
		bindTexture(filmThickTex, "u_ThinFilmThicknessSampler", 18);
		bindTexture(thickTex, "u_ThicknessSampler", 19);
		bindTexture(anisTex, "u_AnisotropySampler", 20);
		bindTexture(dirTex, "u_AnisotropyDirectionSampler", 21);
	}

	meshRenderer::meshRenderer() : renderable() {}

	meshRenderer::meshRenderer(object config) : renderable() {
		copy(config);
		setParent(getPrototype());
	}

	var meshRenderer::draw(list args) {
		if (args.size() < 5)
			return genericError("Mesh draw requires view, camera, lights, environment, and occlusion query");
		auto view = args[0].getUInt16();
		auto cam = args[1].getObject<camera>();
		auto lights = args[2].getList();
		auto env = args[3].getObject<envMap>();
		auto occ = args[4].getObject<occlusionQuery>();

		auto parentObject = getObject<entity>("object");
		auto parentTrans = parentObject.getTransform();
		auto mtx = parentTrans.getWorldMatrix();

		// The PBR program works in world space: upload the camera's
		// view-projection once per draw call (the view id's own
		// bgfx::setViewTransform feeds u_viewProj, which this shader
		// does not read — it declares u_ViewProjectionMatrix itself).
		if (cam) {
			auto camTrans = cam.getTransform();
			auto camMtx = camTrans.getWorldMatrix();
			auto eye = camMtx * vec4f(0, 0, 0, 1);
			auto at = camMtx * vec4f(0, 0, 1, 1);
			auto viewM = lookAt(eye, at);
			auto size = cam.getVar("size");
			auto width = size.getFloat(0);
			auto height = size.getFloat(1);
			auto ratio = height != 0.0f ? width / height : 1.0f;
			auto homo = bgfx::getCaps()->homogeneousDepth;
			auto projM = projection(
				cam.getFloat("fov"), ratio, cam.getFloat("near"),
				cam.getFloat("far"), homo);
			auto viewProj = viewM * projM;
			shaderProgram::createUniform(
				"u_ViewProjectionMatrix", uniformType::Mat4);
			shaderProgram::setUniform(
				"u_ViewProjectionMatrix", viewProj.getPtr());
		}

		// The punctual lights: one u_Lights[] slot (LIGHT_COUNT=1) —
		// the first light component, laid out per uniforms.sh:
		// row0 dir.xyz+range, row1 color.rgb+intensity,
		// row2 pos.xyz+innerCone, row3 outerCone+type.
		{
			float lightMtx[16];
			memset(lightMtx, 0, sizeof(lightMtx));
			// A zeroed light is not inert: the shader normalizes the
			// direction, and normalize(0) poisons every fragment with
			// NaN (NaN framebuffer writes are undefined — on this GL
			// they leave the clear color untouched, hiding the mesh).
			// Give the dead light a valid direction instead.
			lightMtx[2] = -1.0f;
			if (lights && lights.size() > 0) {
				auto li = lights[0].getObject<light>();
				if (li) {
					auto typeName = li.getString("type");
					auto lightObj = li.getObject<entity>("object");
					auto lTrans = lightObj.getTransform();
					auto lMtx = lTrans.getWorldMatrix();
					auto pos = lMtx * vec4f(0, 0, 0, 1);
					auto fwd = lMtx * vec4f(0, 0, 1, 1);
					auto dir = vec3f(
						fwd.getFloat(0) - pos.getFloat(0),
						fwd.getFloat(1) - pos.getFloat(1),
						fwd.getFloat(2) - pos.getFloat(2));
					auto c = li.getUInt32("color", 0xffffffff);
					lightMtx[0] = dir.getFloat(0);
					lightMtx[1] = dir.getFloat(1);
					lightMtx[2] = dir.getFloat(2);
					// lightMtx[3] = range — 0 means unlimited.
					lightMtx[4] = ((c >> 16) & 0xff) / 255.0f;
					lightMtx[5] = ((c >> 8) & 0xff) / 255.0f;
					lightMtx[6] = (c & 0xff) / 255.0f;
					lightMtx[7] = li.getFloat("intensity", 1.0f);
					lightMtx[8] = pos.getFloat(0);
					lightMtx[9] = pos.getFloat(1);
					lightMtx[10] = pos.getFloat(2);
					auto cone = li.getVar("cone");
					lightMtx[11] = cone.getFloat(0);
					lightMtx[12] = cone.getFloat(1);
					lightMtx[13] =
						typeName == "directional" ? 0.0f
						: typeName == "spot" ? 2.0f
											 : 1.0f;
				}
			}
			shaderProgram::createUniform(
				"u_Lights", uniformType::Mat4, 1);
			shaderProgram::setUniform("u_Lights", lightMtx, 1);
		}

		auto meshD = getObject<mesh>("mesh");
		auto nodes = meshD.getList("nodes");
		auto meshes = meshD.getList("meshes");
		function<void(object&, var)> renderNode =
			[&](object& node, var parentMtx) {
				// Node-local TRS (glTF): composed row-major; the
				// accumulated chain multiplies left, so children nest
				// inside their parents.
				var model = parentMtx;
				{
					auto mVar = node.getVar("matrix");
					if (mVar.isList() && mVar.getList().size() == 16) {
						// glTF's node.matrix is column-major in the
						// file; stored verbatim it reads as the
						// equivalent row-major gold matrix (the same
						// transpose duality the shader upload relies
						// on), so no transpose here.
						auto mv = mVar.getList();
						model = mat4x4f({
							mv.getFloat(0), mv.getFloat(1),
							mv.getFloat(2), mv.getFloat(3),
							mv.getFloat(4), mv.getFloat(5),
							mv.getFloat(6), mv.getFloat(7),
							mv.getFloat(8), mv.getFloat(9),
							mv.getFloat(10), mv.getFloat(11),
							mv.getFloat(12), mv.getFloat(13),
							mv.getFloat(14), mv.getFloat(15)}) * parentMtx;
					} else {
						auto t = node.getVar("translation");
						auto r = node.getVar("rotation");
						auto s = node.getVar("scale");
						if (t.isVec3() || r.isQuat() || s.isVec3()) {
							float rt[16];
							bx::mtxFromQuaternion(
								rt,
								r.isQuat()
									? bx::Quaternion(
										{r.getFloat(0), r.getFloat(1),
										 r.getFloat(2), r.getFloat(3)})
									: bx::Quaternion(
										{0.0f, 0.0f, 0.0f, 1.0f}),
								t.isVec3()
									? bx::Vec3(
										t.getFloat(0), t.getFloat(1),
										t.getFloat(2))
									: bx::Vec3(0.0f, 0.0f, 0.0f));
							float sc[16];
							bx::mtxScale(
								sc,
								s.isVec3() ? s.getFloat(0) : 1.0f,
								s.isVec3() ? s.getFloat(1) : 1.0f,
								s.isVec3() ? s.getFloat(2) : 1.0f);
							float local[16];
							bx::mtxMul(local, sc, rt);
							model = mat4x4f({
								local[0], local[1], local[2], local[3],
								local[4], local[5], local[6], local[7],
								local[8], local[9], local[10], local[11],
								local[12], local[13], local[14],
								local[15]}) * parentMtx;
						}
					}
				}
				auto meshId = node.getUInt64("mesh", UINT64_MAX);
				auto mesh = meshes.getObject(meshId);
				auto children = node.getList("children");
				if (mesh) {
					// Buffer handles key off the NODE's name — the
					// mesh's own name is unrelated (meshes are shared).
					auto name = node.getString("name");
					auto primitives = mesh.getList("primitives");
					for (size_t i = 0; i < primitives.size(); ++i) {
						auto prim = primitives[i].getObject();
						auto program =
							prim.getObject<shaderProgram>("program");
						if (program) {
							auto vbh = meshD.getVertexBufferHandle({name, i})
											 .getObject<vertexBuffer>();
							auto ibh = meshD.getIndexBufferHandle({name, i})
											 .getObject<indexBuffer>();
							vbh.set(0);
							ibh.set();
							// Model + normal (inverse-transpose)
							// matrices: gold matrices are row-major bx
							// floats; the shader's column-major read
							// sees the transpose, which is exactly the
							// transform the GLSL expects.
							shaderProgram::createUniform(
								"u_ModelMatrix", uniformType::Mat4);
							shaderProgram::setUniform(
								"u_ModelMatrix", model.getPtr());
							float nm[16];
							bx::mtxInverse(nm, (const float*)model.getPtr());
							float nmT[16];
							bx::mtxTranspose(nmT, nm);
							shaderProgram::createUniform(
								"u_NormalMatrix", uniformType::Mat4);
							shaderProgram::setUniform("u_NormalMatrix", nmT);
							setMaterial(cam, prim, mesh);
							auto type = string("");
							auto mode = prim.getUInt16("mode", 4);
							if (mode == 0)
								type = "points";
							else if (mode == 1)
								type = "lines";
							else if (mode == 2)
								type = "lines";
							else if (mode == 3)
								type = "linestrip";
							auto state = obj({
								{"type", type},
								{"MSAA", true},
								// "ccw" shows the outside faces (the camera
								// chain's handedness flips glTF's authored
								// winding); doubleSided materials opt out
								// below.
								{"cull", "ccw"},
							});
							auto materials = meshD.getList("materials");
							auto material = materials.getObject(
								prim.getUInt64("material", UINT64_MAX));
							auto alphaMode = material.getString("alphaMode", "OPAQUE");
							if (alphaMode == "BLEND") {
								state.setString("blendSrc", "src_alpha");
								state.setString("blendDst", "inv_src_alpha");
							} else if (alphaMode == "MASK") {
								state.setUInt8("alphaRef", uint8_t(
									material.getFloat("alphaCutoff", 0.5f) * 255.0f));
							}
							if (material.getBool("doubleSided", false))
								state.setString("cull", "none");
							program.setState(state);
							program.submit(view);
						}
					}
				}
				if (children && children.size() > 0) {
					for (auto it = children.begin(); it != children.end();
							 ++it) {
						auto node =
							nodes[it->getUInt64(UINT64_MAX)].getObject();
						renderNode(node, model);
					}
				}
			};
		auto nodeName = getString("node");
		object node;
		for (auto it = nodes.begin(); it != nodes.end(); ++it) {
			auto nit = it->getObject();
			if (nodeName.compare(nit.getString("name")) == 0) {
				node = nit;
				break;
			}
		}
		// No "node" configured: render the whole scene graph from its
		// roots (nodes no other node claims as a child), so simple
		// setups don't need to know names.
		if (!node) {
			set<uint64_t> claimed;
			for (auto it = nodes.begin(); it != nodes.end(); ++it)
				for (auto& c : it->getObject().getList("children"))
					claimed.insert(c.getUInt64());
			size_t index = 0;
			for (auto it = nodes.begin(); it != nodes.end();
					 ++it, ++index)
				if (!claimed.count(index)) {
					auto root = it->getObject();
					renderNode(root, mtx);
				}
		} else
			renderNode(node, mtx);
		return var();
	}

	string meshRenderer::gatherDefines(
		object primitive, object meshEntry) {
		// Asset-level lists live on the mesh object; the mesh ENTRY
		// (primitives/weights) is what this call receives. Reading them
		// off the renderer itself found nothing and produced shaders
		// with no material defines at all.
		auto meshAsset = getObject<mesh>("mesh");
		auto matId = primitive.getUInt64("material", UINT64_MAX);
		auto mat = meshAsset.getList("materials").getObject(matId);
		auto atts = primitive.getObject("attributes");
		auto targets = primitive.getList("targets");
		if (!targets) targets = primitive.getList("target");
		auto accessors = meshAsset.getList("accessors");
		auto weights = meshEntry.getList("weights");
		auto extensions = mat.getObject("extensions");
		auto defines = string();

		// One punctual light slot always: u_Lights[LIGHT_COUNT] is a
		// shader compile-time array, and lighting cost with a zeroed
		// (intensity 0) light is negligible.
		defines += "USE_PUNCTUAL=1;LIGHT_COUNT=1;";

		// Renderer config {"unlit", true}: force the material down the
		// KHR_materials_unlit path (flat texture, no shading) — the
		// classic display-model look.
		if (getBool("unlit")) defines += "MATERIAL_UNLIT=1;";

		auto parseTextureInfo = [&](object tex, string key) {
			auto extensions = tex.getObject("extensions");
			if (extensions) {
				auto texTrans =
					extensions.getObject("KHR_texture_transform");
				if (texTrans) {
					defines += "HAS_" + key + "_UV_TRANSFORM=1;";
				}
			}
		};

		auto mode = mat.getString("alphaMode");
		if (mode.compare("MASK") == 0)
			defines += "ALPHAMODE_MASK=1;";
		else if (mode.compare("OPAQUE") == 0)
			defines += "ALPHAMODE_OPAQUE=1;";

		auto unLitObj = extensions.getObject("KHR_materials_unlit");
		if (unLitObj) {
			defines += "MATERIAL_UNLIT=1;";
		}

		auto specGloObj = extensions.getObject(
			"KHR_materials_pbrSpecularGlossiness");
		if (specGloObj) {
			defines += "MATERIAL_SPECULARGLOSSINESS=1;";
		}

		auto clearObj =
			extensions.getObject("KHR_materials_clearcoat");
		if (clearObj) {
			defines += "MATERIAL_CLEARCOAT=1;";
			auto clCoTex = mat.getObject("clearcoatTexture");
			if (clCoTex) {
				defines += "HAS_CLEARCOAT_TEXTURE_MAP=1;";
				parseTextureInfo(clCoTex, "CLEARCOAT");
			}
			auto clRoTex = mat.getObject("clearcoatRoughnessTexture");
			if (clRoTex) {
				defines += "HAS_CLEARCOAT_ROUGHNESS_MAP=1;";
				parseTextureInfo(clRoTex, "CLEARCOATROUGHNESS");
			}
			auto clNoTex = mat.getObject("clearcoatNormalTexture");
			if (clNoTex) {
				defines += "HAS_CLEARCOAT_NORMAL_MAP=1;";
				parseTextureInfo(clNoTex, "CLEARCOATNORMAL");
			}
		}

		auto sheenObj = extensions.getObject("KHR_materials_sheen");
		if (sheenObj) {
			defines += "MATERIAL_SHEEN=1;";
			auto coInTex = mat.getObject("colorIntensityTexture");
			if (coInTex) {
				defines += "HAS_SHEEN_COLOR_INTENSITY_MAP=1;";
				parseTextureInfo(coInTex, "SHEENCOLORINTENSITY");
			}
		}

		auto specObj =
			extensions.getObject("KHR_materials_specular");
		if (specObj) {
			auto metRoSpecTex =
				mat.getObject("metallicRoughnessSpecularTexture");
			if (metRoSpecTex) {
				defines +=
					"MATERIAL_METALLICROUGHNESS_SPECULAROVERRIDE=1;"
					"HAS_METALLICROUGHNESS_SPECULAROVERRIDE_MAP=1;";
				parseTextureInfo(
					metRoSpecTex, "METALLICROUGHNESSSPECULAR");
			}
		}

		auto subSurObj =
			extensions.getObject("KHR_materials_subsurface");
		if (subSurObj) {
			auto subCoTex = mat.getObject("subsurfaceColorTexture");
			auto subThTex =
				mat.getObject("subsurfaceThicknessTexture");
			defines += "MATERIAL_SUBSURFACE=1;";
			if (subCoTex) {
				defines += "HAS_SUBSURFACE_COLOR_MAP=1;";
				parseTextureInfo(subCoTex, "SUBSURFACECOLOR");
			}
			if (subThTex) {
				defines += "HAS_SUBSURFACE_THICKNESS_MAP=1;";
				parseTextureInfo(subThTex, "SUBSURFACETHICKNESS");
			}
		}

		auto anisObj =
			extensions.getObject("KHR_materials_anisotropy");
		if (anisObj) {
			defines += "MATERIAL_ANISOTROPY=1;";
			auto anisTex = mat.getObject("anisotropyTexture");
			auto dirTex = mat.getObject("anisotropyDirectionTexture");
			if (anisTex) {
				defines += "HAS_ANISOTROPY_MAP=1;";
				parseTextureInfo(anisTex, "ANISOTROPY");
			}
			if (dirTex) {
				defines += "HAS_ANISOTROPY_DIRECTION_MAP=1;";
				parseTextureInfo(dirTex, "ANISOTROPYDIRECTION");
			}
		}

		auto thinObj =
			extensions.getObject("KHR_materials_thinfilm");
		if (thinObj) {
			defines += "MATERIAL_THIN_FILM=1;";
			auto thinTex = mat.getObject("thinfilmTexture");
			auto thickTex = mat.getObject("thinfilmThicknessTexture");
			if (thinTex) {
				defines += "HAS_THIN_FILM_MAP=1;";
				parseTextureInfo(thinTex, "THINFILM");
			}
			if (thickTex) {
				defines += "HAS_THIN_FILM_THICKNESS_MAP=1;";
				parseTextureInfo(thickTex, "THINFILMTHICKNESS");
			}
		}

		auto thickObj =
			extensions.getObject("KHR_materials_thickness");
		if (thickObj) {
			defines += "MATERIAL_THICKNESS=1;";
			auto thickTex = mat.getObject("thicknessTexture");
			if (thickTex) {
				defines += "HAS_THICKNESS_MAP=1;";
				parseTextureInfo(thickTex, "THICKNESS");
			}
		}

		auto iorObj = extensions.getObject("KHR_materials_ior");
		if (iorObj) {
			defines += "MATERIAL_IOR=1;";
		}

		auto absObj =
			extensions.getObject("KHR_materials_absorption");
		if (absObj) {
			defines += "MATERIAL_ABSORPTION=1";
		}

		auto transObj =
			extensions.getObject("KHR_materials_transmission");
		if (transObj) {
			defines += "MATERIAL_TRANSMISSION=1;";
		}

		auto difTex = mat.getObject("diffuseTexture");
		if (difTex) {
			defines += "HAS_DIFFUSE_MAP=1;";
			parseTextureInfo(difTex, "DIFFUSE");
		}

		auto specTex = mat.getObject("specularGlossinessTexture");
		if (specTex) {
			defines += "HAS_SPECULAR_GLOSSINESS_MAP=1;";
			parseTextureInfo(specTex, "SPECULARGLOSSINESS");
		}

		auto occTex = mat.getObject("occlusionTexture");
		if (occTex) {
			defines += "HAS_OCCLUSION_MAP=1;";
			parseTextureInfo(occTex, "OCCLUSION");
		}

		auto normTex = mat.getObject("normalTexture");
		if (normTex) {
			defines += "HAS_NORMAL_MAP=1;";
			parseTextureInfo(normTex, "NORMAL");
		}

		auto emTex = mat.getObject("emissiveTexture");
		if (emTex) {
			defines += "HAS_EMISSIVE_MAP=1;";
			parseTextureInfo(emTex, "EMISSIVE");
		}

		auto pbrMetRo = mat.getObject("pbrMetallicRoughness");
		if (pbrMetRo) {
			auto base = pbrMetRo.getObject("baseColorTexture");
			auto metTex =
				pbrMetRo.getObject("metallicRoughnessTexture");
			defines += "MATERIAL_METALLICROUGHNESS=1;";
			if (base) {
				defines += "HAS_BASE_COLOR_MAP=1;";
				parseTextureInfo(base, "BASECOLOR");
			}
			if (metTex) {
				defines += "HAS_METALLIC_ROUGHNESS_MAP=1;";
				parseTextureInfo(metTex, "METALLICROUGHNESS");
			}
		}

		if (atts.getUInt64("TANGENT", UINT64_MAX) != UINT64_MAX)
			defines += "HAS_TANGENTS=1;";
		if (atts.getUInt64("NORMAL", UINT64_MAX) != UINT64_MAX)
			defines += "HAS_NORMALS=1;";
		if (atts.getUInt64("TEXCOORD_0", UINT64_MAX) != UINT64_MAX)
			defines += "HAS_UV_SET1=1;";
		if (atts.getUInt64("TEXCOORD_1", UINT64_MAX) != UINT64_MAX)
			defines += "HAS_UV_SET2=1;";
		auto color0Index = atts.getUInt64("COLOR_0", UINT64_MAX);
		if (color0Index != UINT64_MAX) {
			auto accessor = accessors.getObject(color0Index);
			auto type = accessor.getString("type");
			if (type.compare("VEC3") == 0)
				defines += "HAS_VERTEX_COLOR_VEC3=1;";
			else if (type.compare("VEC4") == 0)
				defines += "HAS_VERTEX_COLOR_VEC4=1;";
		}
		auto useSkinning = false;
		if (atts.getUInt64("JOINTS_0", UINT64_MAX) != UINT64_MAX) {
			useSkinning = true;
			defines += "HAS_JOINT_SET1=1;";
		}
		if (atts.getUInt64("JOINTS_1", UINT64_MAX) != UINT64_MAX) {
			useSkinning = true;
			defines += "HAS_JOINT_SET2=1;";
		}
		if (atts.getUInt64("WEIGHTS_0", UINT64_MAX) != UINT64_MAX) {
			useSkinning = true;
			defines += "HAS_WEIGHT_SET1=1;";
		}
		if (atts.getUInt64("WEIGHTS_1", UINT64_MAX) != UINT64_MAX) {
			useSkinning = true;
			defines += "HAS_WEIGHT_SET2=1;";
		}
		if (useSkinning) defines += "USE_SKINNING=1;";
		// USE_SKINNING

		if (targets && targets.size() > 0) {
			if (weights.size() >= 2) {
				defines += "USE_MORPHING=1;";
				uint8_t i = 0;
				for (auto it = targets.begin(); it != targets.end();
						 ++it, ++i) {
					auto targetObj = it->getObject();
					for (auto tit = targetObj.begin();
							 tit != targetObj.end();
							 ++tit) {
						if (tit->first.compare("POSITION") == 0)
							defines +=
								"HAS_TARGET_POSITION" + to_string(i) + "=1;";
						else if (tit->first.compare("NORMAL") == 0)
							defines +=
								"HAS_TARGET_NORMAL" + to_string(i) + "=1;";
						else if (tit->first.compare("TANGENT") == 0)
							defines +=
								"HAS_TARGET_TANGENT" + to_string(i) + "=1;";
					}
				}
			}
		}

		return defines;
	}

	object meshRenderer::configureVertex(
		object, object, const string& defines) {
		auto ret = object();
		// The varyings' io: expand the varying definition against the
		// assembled defines; the active a_* ids become the shader
		// object's $input list and the v_* ids its $output list — the
		// declarations' types ride on the same definition file via
		// --varyingdef. This finishes the PBR path: configureVertex used
		// to compute the defines and return an empty config, so the
		// generated shader source never declared its varyings and
		// nothing compiled.
		const char* varyingPath = "./shaders/pbr/varying.def.sc";
		const auto expanded = expandVaryingDefinition(varyingPath, defines);
		if (!expanded.empty()) {
			ifstream defineFile(expanded);
			string varyingLine;
			auto inputs = list();
			auto outputs = list();
			while (getline(defineFile, varyingLine)) {
				// "vec3 a_position:POSITION;" / "vec3 v_...:SEM = ...;"
				auto takeIdent = [&](const string& prefix) {
					auto at = varyingLine.find(prefix);
					if (at == string::npos) return string();
					auto end = at + prefix.size();
					while (end < varyingLine.size() &&
						   (isalnum((unsigned char)varyingLine[end]) ||
							   varyingLine[end] == '_'))
						++end;
					return varyingLine.substr(at, end - at);
				};
				const string attrib = takeIdent(" a_");
				if (!attrib.empty()) {
					inputs.pushString(attrib);
					continue;
				}
				const string varying = takeIdent(" v_");
				if (!varying.empty()) outputs.pushString(varying);
			}
			ret.setList("inputs", inputs);
			ret.setList("outputs", outputs);
			ret.setString("defines", defines);
			ret.setString("varying", varyingPath);
		}

		return ret;
	}

	object meshRenderer::configureFragment(
		object, object, const string& defines) {
		auto ret = object();
		// Same io-assembly as the vertex side, from the other end of
		// the varying file AND against the SAME defines: the fragment
		// receives exactly the varyings the vertex stage writes (the
		// v_* lines). Vertex attributes (a_*) are not fragment inputs;
		// listing them compiles to a broken program.
		const char* varyingPath = "./shaders/pbr/varying.def.sc";
		const auto expanded = expandVaryingDefinition(varyingPath, defines);
		if (!expanded.empty()) {
			ifstream defineFile(expanded);
			string varyingLine;
			auto inputs = list();
			while (getline(defineFile, varyingLine)) {
				auto at = varyingLine.find(" v_");
				if (at == string::npos) continue;
				auto end = at + 3;
				while (end < varyingLine.size() &&
					   (isalnum((unsigned char)varyingLine[end]) ||
						   varyingLine[end] == '_'))
					++end;
				inputs.pushString(varyingLine.substr(at + 1, end - at - 1));
			}
			// The fragment reads exactly the varyings the vertex writes
			// (textures.sh was switched to the v_texcoord* names): the
			// stages' io hash must match or createProgram rejects the
			// pair.
			ret.setList("inputs", inputs);
			ret.setString("defines", defines);
			ret.setString("varying", varyingPath);
		}

		return ret;
	}

	var meshRenderer::initialize(list) {
		auto m = getObject<mesh>("mesh");
		auto programs = list({});
		setList("programs", programs);
		// The PBR program's uniform surface: createUniform is
		// name-idempotent, and gold's bindTexture/setUniform only act on
		// registered uniforms — sprite/uiSurface do the same for
		// theirs. Registering a sampler the active defines never
		// reference is harmless.
		for (auto s : {"u_NormalSampler", "u_EmissiveSampler",
			 "u_OcclusionSampler", "u_BaseColorSampler",
			 "u_MetallicRoughnessSampler", "u_DiffuseSampler",
			 "u_SpecularGlossinessSampler", "u_GGXLUT", "u_CharlieLUT",
			 "u_ClearcoatSampler", "u_ClearcoatRoughnessSampler",
			 "u_ClearcoatNormalSampler", "u_SheenColorIntensitySampler",
			 "u_MetallicRoughnessSpecularSampler",
			 "u_SubsurfaceColorSampler", "u_SubsurfaceThicknessSampler",
			 "u_ThinFilmLUT", "u_ThinFilmSampler",
			 "u_ThinFilmThicknessSampler", "u_ThicknessSampler",
			 "u_AnisotropySampler", "u_AnisotropyDirectionSampler",
			 "u_LambertianEnvSampler", "u_GGXEnvSampler",
			 "u_CharlieEnvSampler"})
			shaderProgram::createUniform(s, uniformType::Sampler);
		// The PBR uniform bank travels as five mat4s: on this fork the
		// per-draw vec4 write path zeroes every vec4 while mat4 uniforms
		// land intact (proven via a glUniform logger). Each mat4 row-group
		// row carries one bank slot (see uniforms.sh's macros).
		for (auto s : {"u_PBRBank0", "u_PBRBank1", "u_PBRBank2",
			 "u_PBRBank3", "u_PBRBank4"})
			shaderProgram::createUniform(s, uniformType::Mat4, 1);
		shaderProgram::createUniform(
			"u_ViewProjectionMatrix", uniformType::Mat4);
		shaderProgram::createUniform("u_ModelMatrix", uniformType::Mat4);
		shaderProgram::createUniform("u_NormalMatrix", uniformType::Mat4);
		shaderProgram::createUniform("u_Lights", uniformType::Mat4, 1);
		auto materials = m.getList("materials");
		auto meshes = m.getList("meshes");
		for (auto mit = meshes.begin(); mit != meshes.end();
				 ++mit) {
			auto meshObj = mit->getObject();
			auto meshName = meshObj.getString("name");
			auto primitives = meshObj.getList("primitives");
			for (auto pit = primitives.begin();
					 pit != primitives.end();
					 ++pit) {
				auto primitive = pit->getObject();
				auto matId = primitive.getUInt64("material");
				auto mat = materials.getObject(matId);
				auto matName = mat.getString("name");
				auto cacheName = meshName + matName + "Program";
				auto program = shaderProgram::findInCache(cacheName);
				// One define set per primitive drives BOTH stages: the
				// fragment's varyings and code paths must mirror the
				// vertex side or the program fails to link.
				auto defines = gatherDefines(primitive, meshObj);
				auto vConfig = configureVertex(primitive, meshObj, defines);
				auto fConfig = configureFragment(primitive, meshObj, defines);
				if (!program) {
					// The io/defines ride the shader object's own items:
					// findParent would promote a "proto" key to the
					// parent chain and setParent(getPrototype()) then
					// replaces it, so a config-via-proto never reaches
					// the ctor's lookups (nothing declared, nothing
					// compiled).
					auto vertConfig = obj({
						{"type", 'v'},
						{"src", "./shaders/pbr/vs_pbr.sc"},
					});
					vertConfig.copy(vConfig);
					auto fragConfig = obj({
						{"type", 'f'},
						{"src", "./shaders/pbr/fs_pbr.sc"},
					});
					fragConfig.copy(fConfig);
					program = shaderProgram(obj({
						{"name", matName + "Shader"},
						{"vert", shaderObject(vertConfig)},
						{"frag", shaderObject(fragConfig)},
					}));
				}
				primitive.setObject("program", program);
		}
	}
	return var();
	}

	var meshRenderer::destroy(list) { return var(); }

}  // namespace gold
