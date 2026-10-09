

vec2 getNormalUV()
{
    vec3 uv = vec3(u_NormalUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_NORMAL_UV_TRANSFORM)
    uv *= u_NormalUVTransform;
    #endif

    return uv.xy;
}

vec2 getEmissiveUV()
{
    vec3 uv = vec3(u_EmissiveUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_EMISSIVE_UV_TRANSFORM)
    uv *= u_EmissiveUVTransform;
    #endif

    return uv.xy;
}

vec2 getOcclusionUV()
{
    vec3 uv = vec3(u_OcclusionUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_OCCLUSION_UV_TRANSFORM)
    uv *= u_OcclusionUVTransform;
    #endif

    return uv.xy;
}

vec2 getBaseColorUV()
{
    vec3 uv = vec3(u_BaseColorUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_BASECOLOR_UV_TRANSFORM)
    uv *= u_BaseColorUVTransform;
    #endif

    return uv.xy;
}

vec2 getMetallicRoughnessUV()
{
    vec3 uv = vec3(u_MetallicRoughnessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_METALLICROUGHNESS_UV_TRANSFORM)
    uv *= u_MetallicRoughnessUVTransform;
    #endif

    return uv.xy;
}

vec2 getSpecularGlossinessUV()
{
    vec3 uv = vec3(u_SpecularGlossinessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_SPECULARGLOSSINESS_UV_TRANSFORM)
    uv *= u_SpecularGlossinessUVTransform;
    #endif

    return uv.xy;
}

vec2 getDiffuseUV()
{
    vec3 uv = vec3(u_DiffuseUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_DIFFUSE_UV_TRANSFORM)
    uv *= u_DiffuseUVTransform;
    #endif

    return uv.xy;
}

vec2 getClearcoatUV()
{
    vec3 uv = vec3(u_ClearcoatUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_CLEARCOAT_UV_TRANSFORM)
    uv *= u_ClearcoatUVTransform;
    #endif
    return uv.xy;
}

vec2 getClearcoatRoughnessUV()
{
    vec3 uv = vec3(u_ClearcoatRoughnessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_CLEARCOATROUGHNESS_UV_TRANSFORM)
    uv *= u_ClearcoatRoughnessUVTransform;
    #endif
    return uv.xy;
}

vec2 getClearcoatNormalUV()
{
    vec3 uv = vec3(u_ClearcoatNormalUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_CLEARCOATNORMAL_UV_TRANSFORM)
    uv *= u_ClearcoatNormalUVTransform;
    #endif
    return uv.xy;
}

vec2 getSheenUV()
{
    vec3 uv = vec3(u_SheenColorIntensityUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_SHEENCOLORINTENSITY_UV_TRANSFORM)
    uv *= u_SheenUVTransform;
    #endif
    return uv.xy;
}

vec2 getMetallicRoughnessSpecularUV()
{
    vec3 uv = vec3(u_MetallicRougnessSpecularTextureUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_METALLICROUGHNESSSPECULAR_UV_TRANSFORM)
    uv *= u_MetallicRougnessSpecularUVTransform;
    #endif
    return uv.xy;
}

vec2 getSubsurfaceColorUV()
{
    vec3 uv = vec3(u_SubsurfaceColorUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_SUBSURFACECOLOR_UV_TRANSFORM)
    uv *= u_SubsurfaceColorUVTransform;
    #endif
    return uv.xy;
}

vec2 getSubsurfaceThicknessUV()
{
    vec3 uv = vec3(u_SubsurfaceThicknessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);
    #if defined(HAS_SUBSURFACETHICKNESS_UV_TRANSFORM)
    uv *= u_SubsurfaceThicknessUVTransform;
    #endif
    return uv.xy;
}

vec2 getThinFilmUV()
{
    vec3 uv = vec3(u_ThinFilmUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_THIN_FILM_UV_TRANSFORM)
    uv *= u_ThinFilmUVTransform;
    #endif

    return uv.xy;
}

vec2 getThinFilmThicknessUV()
{
    vec3 uv = vec3(u_ThinFilmThicknessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_THIN_FILM_THICKNESS_UV_TRANSFORM)
    uv *= u_ThinFilmThicknessUVTransform;
    #endif

    return uv.xy;
}

vec2 getThicknessUV()
{
    vec3 uv = vec3(u_ThicknessUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_THICKNESS_UV_TRANSFORM)
    uv *= u_ThicknessUVTransform;
    #endif

    return uv.xy;
}

vec2 getAnisotropyUV()
{
    vec3 uv = vec3(u_AnisotropyUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_ANISOTROPY_UV_TRANSFORM)
    uv *= u_AnisotropyUVTransform;
    #endif

    return uv.xy;
}

vec2 getAnisotropyDirectionUV()
{
    vec3 uv = vec3(u_AnisotropyDirectionUVSet < 1 ? v_texcoord0 : v_texcoord1, 1.0);

    #if defined(HAS_ANISOTROPY_DIRECTION_UV_TRANSFORM)
    uv *= u_AnisotropyDirectionUVTransform;
    #endif

    return uv.xy;
}
