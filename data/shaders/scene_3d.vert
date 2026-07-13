// Vertex stage of the block_3d GPU scene pass (doc/3D_ROADMAP.md Phase 4).
// The CPU delivers everything pre-transformed — clip-space position with a
// real depth, and interpolable light-space coordinates for shadow mapping —
// so this stage is pure passthrough and needs no uniforms.

#version 450

layout(location = 0) in vec4 a_pos;    // NDC x, y; depth z; shadow-receive w
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec3 a_light;  // shadow-map u, v; light depth
layout(location = 4) in float a_vd;    // view depth (world x+y+z), larger = nearer
layout(location = 5) in float a_emit;  // 1.0 for light-emitting geometry

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_light;
layout(location = 3) out float v_recv;
layout(location = 4) out float v_vd;
layout(location = 5) out float v_emit;

void main()
{
    v_color = a_color;
    v_uv = a_uv;
    v_light = a_light;
    v_recv = a_pos.w;
    v_vd = a_vd;
    v_emit = a_emit;
    gl_Position = vec4(a_pos.xy, a_pos.z, 1.0);
}
