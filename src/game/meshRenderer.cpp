#include "meshRenderer.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/timer.h>

#include <cstring>

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
			{"node", ""},
			{"mesh", var()},
			{"proto", renderable::getPrototype()},
		};
		return proto;
	}

	void meshRenderer::setMaterial(
		camera cam, object primitive, object meshObj) {
		auto meshD = getObject<mesh>("mesh");
		auto materials = meshD.getList("materials");
		auto matId = primitive.getUInt64("material", UINT64_MAX);
		auto mat = materials.getObject(matId);
		if (!mat) return;
		auto program = primitive.getObject<shaderProgram>("program");
		auto bindTexture = [&](object info, string sampler, uint8_t stage) {
			if (!info || !program) return;
			auto resolved = info.getObject("resolvedTexture");
			auto image = resolved.getObject("image");
			if (!image) return;
			auto data = image.getBinary("data");
			if (data.empty()) return;
			auto tex = info.getObject<gpuTexture>("gpuTexture");
			if (!tex) {
				tex = gpuTexture(obj({
					{"data", data},
					{"name", sampler + ":" + mat.getString("name")},
				}));
				info.setObject("gpuTexture", tex);
			}
			shaderProgram::bindTexture(sampler, stage, tex);
		};
		auto set = pbrUniformSet();
		memset(&set, 0, sizeof(pbrUniformSet));
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
			auto base = pbrMetRo.getObject("baseColorTexture");
			auto metTex =
				pbrMetRo.getObject("metallicRoughnessTexture");
			bindTexture(base, "u_BaseColorSampler", 0);
			bindTexture(metTex, "u_MetallicRoughnessSampler", 1);
		}
		bindTexture(normTex, "u_NormalSampler", 2);
		bindTexture(occTex, "u_OcclusionSampler", 3);
		bindTexture(emTex, "u_EmissiveSampler", 4);
	}

	meshRenderer::meshRenderer() : renderable() {}

	meshRenderer::meshRenderer(object config) : renderable() {
		copy(config);
		setParent(getPrototype());
	}

	var meshRenderer::draw(list args) {
		auto view = args[0].getUInt16();
		auto cam = args[1].getObject<camera>();
		auto lights = args[2].getList();
		auto env = args[3].getObject<envMap>();
		auto occ = args[4].getObject<occlusionQuery>();

		auto parentObject = getObject<entity>("object");
		auto parentTrans = parentObject.getTransform();
		auto mtx = parentTrans.getWorldMatrix();

		auto meshD = getObject<mesh>("mesh");
		auto nodes = meshD.getList("nodes");
		auto meshes = meshD.getList("meshes");
		function<void(object&)> renderNode = [&](object& node) {
			auto meshId = node.getUInt64("mesh", UINT64_MAX);
			auto mesh = meshes.getObject(meshId);
			auto children = node.getList("children");
			if (mesh) {
				auto name = mesh.getString("name");
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
						program.setState({
							{"type", type},
							{"MSAA", true},
						});
						program.submit(view);
					}
				}
			}
			if (children && children.size() > 0) {
				for (auto it = children.begin(); it != children.end();
						 ++it) {
					auto node =
						nodes[it->getUInt64(UINT64_MAX)].getObject();
					renderNode(node);
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
		if (node) renderNode(node);
		return var();
	}

	object meshRenderer::configureVertex(
		object primitive, object mesh) {
		auto matId = primitive.getUInt64("material");
		auto mat = getList("materials").getObject(matId);
		auto atts = primitive.getObject("primitive");
		auto targets = primitive.getList("target");
		auto accessors = getList("accessors");
		auto weights = getList("weights");
		auto extensions = getObject("extensions");
		auto ret = object();
		auto defines = string();

		auto defineTexTrans = false;
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

		return ret;
	}

	object meshRenderer::configureFragment(
		object primitive, object mesh) {
		auto ret = object();
		auto defines = string();

		return ret;
	}

	var meshRenderer::initialize(list) {
		auto m = getObject<mesh>("mesh");
		auto programs = list({});
		setList("programs", programs);
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
				auto vConfig = configureVertex(primitive, meshObj);
				auto fConfig = configureFragment(primitive, meshObj);
				if (!program) {
					program = shaderProgram(obj({
						{"name", matName + "Shader"},
						{"vert", shaderObject(obj({
							{"type", 'v'},
							{"src", "./shaders/pbr/vs_pbr.sc"},
							{"proto", vConfig},
						}))},
						{"frag", shaderObject(obj({
							{"type", 'f'},
							{"src", "./shaders/pbr/fs_pbr.sc"},
							{"proto", fConfig},
						}))},
					}));
				}
				primitive.setObject("program", program);
		}
	}
	return var();
	}

	var meshRenderer::destroy(list) { return var(); }

}  // namespace gold
