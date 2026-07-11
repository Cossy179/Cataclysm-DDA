// Shadow-map vertex stage of the block_3d GPU scene pass: casters arrive
// already in light space (shadow-map u, v in [0, 1] and normalized light
// depth), so this just maps them to clip space. The y flip keeps the
// rendered map row-aligned with plain texture(u_shadow, uv) sampling.

#version 450

layout(location = 0) in vec3 a_light;
layout(location = 0) out float v_depth;

void main()
{
    v_depth = a_light.z;
    gl_Position = vec4(a_light.x * 2.0 - 1.0,
                       -(a_light.y * 2.0 - 1.0),
                       a_light.z, 1.0);
}
