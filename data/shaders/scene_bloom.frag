// Bloom for the block_3d GPU scene pass. Reads the main pass's emissive
// mask (BGRA, black except where light-emitting geometry — fire,
// explosions, portal storms — is drawn) and adds a blurred glow onto the
// scene, so bright sources bleed light into their surroundings.
//
// A single-pass approximate Gaussian: several rings of bilinear taps at
// growing radius, weighted by distance. Bilinear sampling on the linear
// sampler lets each tap average a 2x2 emissive neighborhood, widening the
// effective kernel cheaply. The result is additively blended onto the
// scene by the pipeline, so this shader never reads the color it adds to.

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_emissive;

layout(location = 0) out vec4 out_color;

const float INTENSITY = 0.9;
// Ring radii in pixels and their weights (nearer rings contribute more).
// Bilinear taps between rings keep the falloff smooth despite the sparse
// sampling.
const int RINGS = 4;
const float RADII[4] = float[](3.0, 8.0, 15.0, 24.0);
const float RING_W[4] = float[](1.0, 0.7, 0.4, 0.2);

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_emissive, 0));
    vec2 uv = gl_FragCoord.xy * texel;

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
