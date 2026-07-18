// Bloom downsample for the block_3d GPU scene pass: the first stage of the
// two-pass bloom chain. Renders into a half-resolution target, sampling the
// full-resolution emissive mask with a small ring pre-blur (bilinear taps),
// so the composite stage's ring radii — measured in half-res texels — reach
// twice as far across the scene with smooth falloff and no banding.
//
// Output replaces the target (no blending); the composite stage additively
// blends the widened glow onto the scene.

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_emissive;

layout(location = 0) out vec4 out_color;

const int RINGS = 2;
const float RADII[2] = float[](2.0, 5.0);
const float RING_W[2] = float[](0.8, 0.5);

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_emissive, 0));
    // gl_FragCoord is in half-res target space; the emissive is full-res.
    vec2 uv = gl_FragCoord.xy * 2.0 * texel;

    vec3 sum = texture(u_emissive, uv).rgb;
    float wsum = 1.0;
    vec2 dirs[8] = vec2[](vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1),
                          vec2(0.707, 0.707), vec2(-0.707, 0.707),
                          vec2(0.707, -0.707), vec2(-0.707, -0.707));
    for (int r = 0; r < RINGS; r++) {
        for (int i = 0; i < 8; i++) {
            sum += texture(u_emissive, uv + dirs[i] * RADII[r] * texel).rgb * RING_W[r];
            wsum += RING_W[r];
        }
    }
    out_color = vec4(sum / wsum, 1.0);
}
