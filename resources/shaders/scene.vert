#version 440

// C3a — the scene's only vertex stage. Colour is per-vertex rather than
// resolved from a ramp texture here, because classification already happened
// on the CPU: the 3D view and the 2D map read the same LayerStyle, so they
// cannot disagree about what a class is coloured.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 normalMatrix;
    vec4 lightDirection;
    vec4 params;          // x = opacity, y = ambient, z = depth nudge
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    // The normal matrix, not the model matrix: vertical exaggeration scales
    // Z alone, and a normal scaled that way tilts the wrong direction.
    vNormal     = normalize((ubuf.normalMatrix * vec4(normal, 0.0)).xyz);
    vColor      = color;
    gl_Position = ubuf.mvp * vec4(position, 1.0);

    // Coplanar lines are pulled a hair toward the eye, so that a mesh's edges
    // and a draped network are not decided against the very surface they lie
    // on by whichever way the two interpolators rounded. Scaled by w, so the
    // shift is the same in normalised depth at any distance, and zero for
    // everything that is not a line.
    gl_Position.z -= ubuf.params.z * gl_Position.w;
}
