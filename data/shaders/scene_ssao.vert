// Fullscreen-triangle vertex stage for the screen-space AO pass. Emits one
// oversized triangle covering the viewport from gl_VertexIndex alone, so no
// vertex buffer is bound. The fragment stage reads the depth target by
// integer pixel (texelFetch on gl_FragCoord), so no UV is passed through.

#version 450

void main()
{
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
