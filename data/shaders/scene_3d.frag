// Fragment stage of the block_3d GPU scene pass: textured, vertex-lit
// color with per-pixel sun shadows sampled from the shadow map rendered
// by scene_3d_shadow.*. Four-tap PCF softens the shadow edge.

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_atlas;
layout(set = 2, binding = 1) uniform sampler2D u_shadow;

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_light;
layout(location = 3) in float v_recv;
layout(location = 4) in float v_vd;
layout(location = 5) in float v_emit;
layout(location = 0) out vec4 out_color;
// Second render target: view depth (block units) for the screen-space AO
// pass. Background stays at the far sentinel this target is cleared to.
layout(location = 1) out float out_depth;
// Third render target: emissive color for the bloom pass, black except
// where light-emitting geometry is drawn.
layout(location = 2) out vec4 out_emissive;

// Depth bias in normalized light-depth units, against shadow acne on
// faces nearly parallel to the sun.
const float BIAS = 0.0035;
// How dark full shadow gets; matches the raster path's nearest-occluder
// shade so toggling the GPU pass is not a lighting rebalance.
const float SHADOW_STRENGTH = 0.45;

void main()
{
    vec4 c = v_color * texture(u_atlas, v_uv);
    if (v_recv > 0.5) {
        vec2 texel = 1.0 / vec2(textureSize(u_shadow, 0));
        // 3x3 PCF: nine taps on a one-texel grid soften the shadow edge.
        float occluded = 0.0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                vec2 off = vec2(float(dx), float(dy)) * texel;
                float nearest = texture(u_shadow, v_light.xy + off).r;
                occluded += (v_light.z - BIAS > nearest) ? 1.0 : 0.0;
            }
        }
        c.rgb *= 1.0 - SHADOW_STRENGTH * (occluded / 9.0);
    }
    out_color = c;
    out_depth = v_vd;
    out_emissive = v_emit > 0.5 ? vec4(c.rgb * c.a, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
