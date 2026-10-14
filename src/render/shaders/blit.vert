#version 440

layout(location = 0) out vec2 vUV;

layout(std140, binding = 0) uniform buf {
    vec4 flip; // x = inverter v
} ubuf;

void main()
{
    // Triângulo que cobre a viewport inteira.
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = pos;
    if (ubuf.flip.x > 0.5)
        vUV.y = 1.0 - vUV.y;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
