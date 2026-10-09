
SAMPLER2D(u_NormalSampler, 0);
SAMPLER2D(u_EmissiveSampler, 1);
SAMPLER2D(u_OcclusionSampler, 2);
SAMPLER2D(u_BaseColorSampler, 3);
SAMPLER2D(u_MetallicRoughnessSampler, 4);
SAMPLER2D(u_DiffuseSampler, 5);
SAMPLER2D(u_SpecularGlossinessSampler, 6);
SAMPLER2D(u_GGXLUT, 7);
SAMPLER2D(u_CharlieLUT, 8);
SAMPLER2D(u_ClearcoatSampler, 9);
SAMPLER2D(u_ClearcoatRoughnessSampler, 10);
SAMPLER2D(u_ClearcoatNormalSampler, 11);
SAMPLER2D(u_SheenColorIntensitySampler, 12);
SAMPLER2D(u_MetallicRoughnessSpecularSampler, 13);
SAMPLER2D(u_SubsurfaceColorSampler, 14);
SAMPLER2D(u_SubsurfaceThicknessSampler, 15);
SAMPLER2D(u_ThinFilmLUT, 16);
SAMPLER2D(u_ThinFilmSampler, 17);
SAMPLER2D(u_ThinFilmThicknessSampler, 18);
SAMPLER2D(u_ThicknessSampler, 19);
SAMPLER2D(u_AnisotropySampler, 20);
SAMPLER2D(u_AnisotropyDirectionSampler, 21);

SAMPLERCUBE(u_LambertianEnvSampler, 22);
SAMPLERCUBE(u_GGXEnvSampler, 23);
SAMPLERCUBE(u_CharlieEnvSampler, 24);

// One vec4 per PBR uniform-bank slot. The upstream uniform bank
// (a vec4 array) never arrives through this bgfx fork's per-draw uniform
// path (individually-named vec4s do), so the macros below alias onto
// individually-uploaded uniforms.
// Five mat4s carry the 20 PBR vec4 slots. This fork's per-draw write
// path zeroes every vec4 uniform while mat4s land intact, so the bank
// travels as matrices; each macro below reads one row.
uniform mat4 u_PBRBank0;
uniform mat4 u_PBRBank1;
uniform mat4 u_PBRBank2;
uniform mat4 u_PBRBank3;
uniform mat4 u_PBRBank4;

uniform mat3 u_NormalUVTransform;
uniform mat3 u_EmissiveUVTransform;
uniform mat3 u_OcclusionUVTransform;
uniform mat3 u_BaseColorUVTransform;
uniform mat3 u_MetallicRoughnessUVTransform;
uniform mat3 u_SpecularGlossinessUVTransform;
uniform mat3 u_DiffuseUVTransform;
uniform mat3 u_ClearcoatUVTransform;
uniform mat3 u_ClearcoatRoughnessUVTransform;
uniform mat3 u_ClearcoatNormalUVTransform;
uniform mat3 u_SheenColorIntensityUVTransform;
uniform mat3 u_MetallicRougnessSpecularUVTransform;
uniform mat3 u_SubsurfaceColorUVTransform;
uniform mat3 u_SubsurfaceThicknessUVTransform;
uniform mat3 u_ThinFilmUVTransform;
uniform mat3 u_ThinFilmThicknessUVTransform;
uniform mat3 u_ThicknessUVTransform;
uniform mat3 u_AnisotropyUVTransform;
uniform mat3 u_AnisotropyDirectionUVTransform;
uniform mat4 u_ViewProjectionMatrix;
uniform mat4 u_ModelMatrix;
uniform mat4 u_NormalMatrix;



#define u_MipCount u_PBRBank0[0].x
#define u_OcclusionStrength u_PBRBank0[0].y
#define u_NormalScale u_PBRBank0[0].z
#define u_Exposure u_PBRBank0[0].w

#define u_SheenRoughness u_PBRBank0[1].x
#define u_Anisotropy u_PBRBank0[1].y
#define u_SubsurfaceScale u_PBRBank0[1].z
#define u_SubsurfaceDistortion u_PBRBank0[1].w

#define u_SubsurfacePower u_PBRBank0[2].x
#define u_ThinFilmThicknessMinimum u_PBRBank0[2].y
#define u_ThinFilmThicknessMaximum u_PBRBank0[2].z
#define u_Thickness u_PBRBank0[2].w

#define u_NormalUVSet u_PBRBank0[3].x
#define u_EmissiveUVSet u_PBRBank0[3].y
#define u_OcclusionUVSet u_PBRBank0[3].z
#define u_BaseColorUVSet u_PBRBank0[3].w

#define u_MetallicRoughnessUVSet u_PBRBank1[0].x
#define u_DiffuseUVSet u_PBRBank1[0].y
#define u_SpecularGlossinessUVSet u_PBRBank1[0].z
#define u_ClearcoatUVSet u_PBRBank1[0].w

#define u_ClearcoatRoughnessUVSet u_PBRBank1[1].x
#define u_ClearcoatNormalUVSet u_PBRBank1[1].y
#define u_SheenColorIntensityUVSet u_PBRBank1[1].z
#define u_MetallicRougnessSpecularTextureUVSet u_PBRBank1[1].w

#define u_SubsurfaceColorUVSet u_PBRBank1[2].x
#define u_SubsurfaceThicknessUVSet u_PBRBank1[2].y
#define u_ThinFilmUVSet u_PBRBank1[2].z
#define u_ThinFilmThicknessUVSet u_PBRBank1[2].w

#define u_ThicknessUVSet u_PBRBank1[3].x
#define u_AnisotropyUVSet u_PBRBank1[3].y
#define u_AnisotropyDirectionUVSet u_PBRBank1[3].z
#define u_MetallicFactor u_PBRBank1[3].w

#define u_RoughnessFactor u_PBRBank2[0].x
#define u_GlossinessFactor u_PBRBank2[0].y
#define u_SheenIntensityFactor u_PBRBank2[0].z
#define u_ClearcoatFactor u_PBRBank2[0].w

#define u_ClearcoatRoughnessFactor u_PBRBank2[1].x
#define u_MetallicRoughnessSpecularFactor u_PBRBank2[1].y
#define u_SubsurfaceThicknessFactor u_PBRBank2[1].z
#define u_ThinFilmFactor u_PBRBank2[1].w

#define u_Transmission u_PBRBank2[2].x
#define u_AlphaCutoff u_PBRBank2[2].y
#define u_IOR_and_f0 u_PBRBank2[2].zw

#define u_BaseColorFactor u_PBRBank2[3]
#define u_DiffuseFactor u_PBRBank3[0]
#define u_SpecularFactor u_PBRBank3[1].xyz
#define u_SheenColorFactor u_PBRBank3[2].xyz
#define u_AnisotropyDirection u_PBRBank3[3].xyz
#define u_SubsurfaceColorFactor u_PBRBank4[0].xyz
#define u_AbsorptionColor u_PBRBank4[1].xyz
#define u_Camera u_PBRBank4[2].xyz
#define u_EmissiveFactor u_PBRBank4[3].xyz

#if defined(USE_MORPHING)
uniform vec4 u_morphWeights[WEIGHT_COUNT];
#endif

#if defined(USE_SKINNING)
uniform mat4 u_jointMatrix[JOINT_COUNT];
uniform mat4 u_jointNormalMatrix[JOINT_COUNT];
#endif

#if defined(USE_PUNCTUAL)
/*
dx, dy, dz, r,
cr, cg, cb, i,
px, py, pz, ic,
oc, t, n/a, n/a
*/
uniform mat4 u_Lights[LIGHT_COUNT];
#endif