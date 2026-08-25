#version 440

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 normalMatrix;
    vec4 lightDirection;
    vec4 params;          // x = opacity, y = ambient, z = depth nudge
    vec4 texMap;          // xy = west/north corner, zw = 1 / extent size
} ubuf;

layout(binding = 1) uniform sampler2D groundTexture;

void main()
{
    // Two-sided, as scene.frag is: a ground plane seen from below is still
    // the ground, and going black there reads as a hole rather than as a
    // lighting choice.
    float lambert = abs(dot(normalize(vNormal), normalize(ubuf.lightDirection.xyz)));
    float shade   = ubuf.params.y + (1.0 - ubuf.params.y) * lambert;

    // The image arrives premultiplied, which is what the target's blend state
    // expects, so its own alpha is already folded into its colour. A raster
    // is transparent wherever it has no data, and that transparency has to
    // survive: painting no-data as black would invent ground.
    vec4 texel = texture(groundTexture, vTexCoord);
    float alpha = texel.a * vColor.a * ubuf.params.x;

    fragColor = vec4(texel.rgb * shade * vColor.rgb * ubuf.params.x, alpha);
}
