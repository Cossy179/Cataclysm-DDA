// Bloom composite for the block_3d GPU scene pass: the second stage of the
// two-pass bloom chain. Reads the half-resolution pre-blurred emissive from
// scene_bloom_down.frag and adds a blurred glow onto the scene, so bright
// sources (fire, explosions, portal storms) bleed light into their
// surroundings.
//
// An approximate Gaussian: several rings of bilinear taps at growing
// radius, weighted by distance. The source being half-res doubles every
// ring's reach in scene pixels, and its pre-blur keeps the falloff smooth
// despite the sparse sampling. The result is additively blended onto the
// scene by the pipeline, so this shader never reads the color it adds to.

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_emissive;

layout(location = 0) out vec4 out_color;

const float INTENSITY = 0.85;
// Ring radii in half-res texels (double that in scene pixels).
const int RINGS = 4;
const float RADII[4] = float[](3.0, 8.0, 15.0, 24.0);
const float RING_W[4] = float[](1.0, 0.7, 0.4, 0.2);

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_emissive, 0));
    // gl_FragCoord is in full-res scene space; the source is half-res.
    vec2 uv = gl_FragCoord.xy * texel * 0.5;

    // Center tap plus eight directions per ring.
    vec3 sum = texture(u_emissive, uv).rgb;
    float wsum = 1.0;
    vec2 dirs[8] = vec2[](vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1),
                          vec2(0.707, 0.707), vec2(-0.707, 0.707),
                          vec2(0.707, -0.707), vec2(-0.707, -0.707));
    for (int r = 0; r < RINGS; r++) {
        float w = RING_W[r];
        for (int i = 0; i < 8; i++) {
            sum += texture(u_emissive, uv + dirs[i] * RADII[r] * texel).rgb * w;
            wsum += w;
        }
    }
    out_color = vec4(sum / wsum * INTENSITY, 1.0);
}
