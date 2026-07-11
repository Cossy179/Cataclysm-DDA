// Shadow-map fragment stage: store the light depth of the nearest caster
// in an R32F color target (the pass's own depth buffer resolves nearest).

#version 450

layout(location = 0) in float v_depth;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(v_depth, 0.0, 0.0, 1.0);
}
