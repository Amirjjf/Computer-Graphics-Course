#version 330 core

// Vertex attributes
in vec3 aPosition;
in vec3 aNormal;

// Varyings to the fragment shader
out vec4 vColor;       // used in basic mode (bShading==0)
out vec3 vWorldPos;    // world-space position for fancy shading
out vec3 vNormal;      // world-space normal for fancy shading

// Uniforms
uniform mat4 uModelToWorld;
uniform mat4 uWorldToClip;
uniform int  bShading;
uniform mat3 uNormalMatrix; // transforms object-space normals to world-space

const vec3 distinctColors[6] = vec3[6](
    vec3(0, 0, 1), vec3(0, 1, 0), vec3(0, 1, 1),
    vec3(1, 0, 0), vec3(1, 0, 1), vec3(1, 1, 0));

void main()
{
    // Compute world-space position and normal for per-fragment shading
    vec4 worldPos4 = uModelToWorld * vec4(aPosition, 1.0);
    vWorldPos = worldPos4.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);

    // In basic mode, pass a distinct vertex color; in fancy mode, fragment shader ignores this
    vColor = vec4(distinctColors[gl_VertexID % 6], 1.0);

    // Final clip-space position
    gl_Position = uWorldToClip * worldPos4;
}
