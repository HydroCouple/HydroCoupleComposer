#version 440

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 normalMatrix;
    vec4 lightDirection;
    vec4 params;          // x = opacity, y = ambient
} ubuf;

void main()
{
    // Two-sided: a mesh read from a file may be wound either way, and a
    // surface that goes black when seen from below reads as a hole in the
    // terrain rather than as a lighting choice.
    float lambert = abs(dot(normalize(vNormal), normalize(ubuf.lightDirection.xyz)));
    float shade   = ubuf.params.y + (1.0 - ubuf.params.y) * lambert;

    // Premultiplied, because that is what the render target's blend state
    // expects; a straight-alpha colour would fringe wherever it is blended.
    float alpha = vColor.a * ubuf.params.x;
    fragColor   = vec4(vColor.rgb * shade * alpha, alpha);
}
