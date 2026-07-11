// FXAA post-process for the block_3d world renderer (doc/3D_ROADMAP.md
// Phase 4). The 3D scene is drawn into an offscreen texture and blitted
// back through this shader, which smooths the hard geometric edges of the
// flat-shaded block world. Classic luma-based FXAA (Lottes) in its compact
// five-tap form; the texel size comes from textureSize so no uniform
// buffer is needed (per-draw uniform uploads are avoided on the SDL3 GPU
// renderer, see cata_shader::variant_pass).

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_atlas;

layout(location = 0) in vec4 v_vertex_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

const float FXAA_SPAN_MAX = 8.0;
const float FXAA_REDUCE_MUL = 1.0 / 8.0;
const float FXAA_REDUCE_MIN = 1.0 / 128.0;

void main()
{
    const vec2 texel = 1.0 / vec2( textureSize( u_atlas, 0 ) );
    const vec3 luma = vec3( 0.299, 0.587, 0.114 );

    vec3 rgb_nw = texture( u_atlas, v_uv + vec2( -1.0, -1.0 ) * texel ).rgb;
    vec3 rgb_ne = texture( u_atlas, v_uv + vec2( 1.0, -1.0 ) * texel ).rgb;
    vec3 rgb_sw = texture( u_atlas, v_uv + vec2( -1.0, 1.0 ) * texel ).rgb;
    vec3 rgb_se = texture( u_atlas, v_uv + vec2( 1.0, 1.0 ) * texel ).rgb;
    vec4 sample_m = texture( u_atlas, v_uv );
    vec3 rgb_m = sample_m.rgb;

    float luma_nw = dot( rgb_nw, luma );
    float luma_ne = dot( rgb_ne, luma );
    float luma_sw = dot( rgb_sw, luma );
    float luma_se = dot( rgb_se, luma );
    float luma_m = dot( rgb_m, luma );

    float luma_min = min( luma_m, min( min( luma_nw, luma_ne ),
                                       min( luma_sw, luma_se ) ) );
    float luma_max = max( luma_m, max( max( luma_nw, luma_ne ),
                                       max( luma_sw, luma_se ) ) );

    vec2 dir = vec2( -( ( luma_nw + luma_ne ) - ( luma_sw + luma_se ) ),
                     ( luma_nw + luma_sw ) - ( luma_ne + luma_se ) );

    float dir_reduce = max( ( luma_nw + luma_ne + luma_sw + luma_se ) *
                            0.25 * FXAA_REDUCE_MUL, FXAA_REDUCE_MIN );
    float rcp_dir_min = 1.0 / ( min( abs( dir.x ), abs( dir.y ) ) + dir_reduce );
    dir = clamp( dir * rcp_dir_min, vec2( -FXAA_SPAN_MAX ),
                 vec2( FXAA_SPAN_MAX ) ) * texel;

    vec3 rgb_a = 0.5 * (
                     texture( u_atlas, v_uv + dir * ( 1.0 / 3.0 - 0.5 ) ).rgb +
                     texture( u_atlas, v_uv + dir * ( 2.0 / 3.0 - 0.5 ) ).rgb );
    vec3 rgb_b = rgb_a * 0.5 + 0.25 * (
                     texture( u_atlas, v_uv + dir * -0.5 ).rgb +
                     texture( u_atlas, v_uv + dir * 0.5 ).rgb );

    float luma_b = dot( rgb_b, luma );
    vec3 rgb_out = ( luma_b < luma_min || luma_b > luma_max ) ? rgb_a : rgb_b;

    out_color = vec4( rgb_out * v_vertex_color.rgb,
                      sample_m.a * v_vertex_color.a );
}
