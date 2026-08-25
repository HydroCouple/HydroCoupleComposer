#version 440

// C3c-2 — the textured ground plane's vertex stage.
//
// The only difference from scene.vert is where the colour comes from: an
// image rather than the vertex. Texture coordinates are derived from world
// position rather than carried per vertex, because a ground plane's are an
// affine function of position — storing them would be storing the same four
// numbers a million times, and re-deriving them every time the drape is
// refined against a finer terrain.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 normalMatrix;
    vec4 lightDirection;
    vec4 params;          // x = opacity, y = ambient, z = depth nudge
    vec4 texMap;          // xy = west/north corner, zw = 1 / extent size
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vNormal     = normalize((ubuf.normalMatrix * vec4(normal, 0.0)).xyz);
    vColor      = color;

    // World Y grows northward and the image's first row is its northern
    // edge, which is the convention MapTransform draws in — hence the
    // subtraction rather than a second offset.
    vTexCoord   = vec2((position.x - ubuf.texMap.x) * ubuf.texMap.z,
                       (ubuf.texMap.y - position.y) * ubuf.texMap.w);

    gl_Position = ubuf.mvp * vec4(position, 1.0);

    // A ground plane is coplanar with the terrain it was draped on, by
    // construction, so it is pulled a step toward the eye for the same reason
    // scene.vert pulls lines: otherwise the two have equal depth and the
    // terrain, drawn first, keeps the pixels.
    gl_Position.z -= ubuf.params.z * gl_Position.w;
}
