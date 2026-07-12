// Screen-space ambient occlusion for the block_3d GPU scene pass. Reads the
// main pass's view-depth target (R32F, block units, larger = nearer the
// camera; background sits at a large-negative sentinel) and darkens creases
// where geometry meets — the concave seams at the base of walls.
//
// The axonometric ground plane is itself a smooth view-depth ramp (depth
// grows toward the camera across a flat floor), so a naive "is my neighbor
// nearer" test would darken everything. Instead this measures local
// concavity with a symmetric second difference per axis: on a flat or
// linearly-sloped surface opposite neighbors average back to the center and
// contribute nothing; only a genuine step (a wall rising beside the floor)
// pushes the pair average nearer than the center and registers occlusion.
// The output is a grey AO factor the pipeline blends multiplicatively onto
// the scene, so this shader never reads the color it modifies.

#version 450

layout(set = 2, binding = 0) uniform sampler2D u_depth;

layout(location = 0) out vec4 out_color;

const float BIAS = 0.08;      // concavity below this is flat-surface noise
const float RANGE = 0.9;      // concavity at/above this is full occlusion
const float STRENGTH = 0.85;
const float SENTINEL = -1.0e8;

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(u_depth, 0);
    float d = texelFetch(u_depth, p, 0).r;
    if (d < SENTINEL) {
        out_color = vec4(1.0);
        return;
    }

    // Four symmetric axis pairs (H, V, both diagonals), sampled at two radii.
    ivec2 dirs[4] = ivec2[](ivec2(1, 0), ivec2(0, 1), ivec2(1, 1), ivec2(1, -1));
    float occ = 0.0;
    float total = 0.0;
    for (int r = 1; r <= 2; r++) {
        for (int i = 0; i < 4; i++) {
            ivec2 off = dirs[i] * (r * 2);
            ivec2 a = p + off;
            ivec2 b = p - off;
            if (a.x < 0 || a.y < 0 || a.x >= size.x || a.y >= size.y ||
                b.x < 0 || b.y < 0 || b.x >= size.x || b.y >= size.y) {
                continue;
            }
            float da = texelFetch(u_depth, a, 0).r;
            float db = texelFetch(u_depth, b, 0).r;
            if (da < SENTINEL || db < SENTINEL) {
                continue;
            }
            total += 1.0;
            // Concavity: pair average nearer than the center means the
            // center sits in a valley between raised geometry.
            float concavity = 0.5 * (da + db) - d;
            if (concavity > BIAS) {
                occ += clamp((concavity - BIAS) / (RANGE - BIAS), 0.0, 1.0);
            }
        }
    }
    float ao = total > 0.0 ? 1.0 - STRENGTH * (occ / total) : 1.0;
    out_color = vec4(vec3(ao), 1.0);
}
