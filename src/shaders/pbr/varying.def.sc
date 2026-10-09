vec3 v_position:POSITION = vec3(0.0, 0.0, 0.0);
vec2 v_texcoord0:TEXCOORD0 = vec2(0.0, 0.0);
vec2 v_texcoord1:TEXCOORD1 = vec2(0.0, 0.0);
#if defined(HAS_NORMALS) && !defined(HAS_TANGENTS)
vec3 v_normal:NORMAL = vec3(0.0, 0.0, 0.0);
#endif
#if defined(HAS_NORMALS) && defined(HAS_TANGENTS)
vec3 v_tbn0:TEXCOORD2 = vec3(0.0, 0.0, 0.0);
vec3 v_tbn1:TEXCOORD3 = vec3(0.0, 0.0, 0.0);
vec3 v_tbn2:TEXCOORD4 = vec3(0.0, 0.0, 0.0);
#endif

vec3 a_position:POSITION;
#if defined(HAS_NORMALS)
vec3 a_normal:NORMAL;
#endif
#if defined(HAS_TANGENTS)
vec4 a_tangent:TANGENT;
#endif
vec2 a_texcoord0:TEXCOORD0;
vec2 a_texcoord1:TEXCOORD1;