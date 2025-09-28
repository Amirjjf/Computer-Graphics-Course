#version 330 core

in vec4 vColor;     // from VS (used when bShading == 0)
in vec3 vWorldPos;  // from VS
in vec3 vNormal;    // from VS
out vec4 fColor;

uniform int   bShading;     // 0 = basic per-vertex color, 1 = fancy shading
uniform vec3  uCameraPos;   // camera position in world-space
uniform float uTime;        // time in seconds for animation

// Helper: apply simple gamma correction from linear -> sRGB
vec3 gammaCorrect(vec3 c) {
    const float invGamma = 1.0/2.2;
    return pow(max(c, 0.0), vec3(invGamma));
}

void main()
{
    if (bShading == 0) {
        // Basic mode: keep original appearance
        fColor = vColor;
        return;
    }

    // Fancy per-fragment shading
    // Two animated point lights with different colors
    vec3 Lpos0 = vec3(1.5 * cos(uTime), 0.8 + 0.3 * sin(0.7 * uTime), 1.5 * sin(uTime));
    vec3 Lcol0 = vec3(1.0, 0.90, 0.70);   // warm
    vec3 Lpos1 = vec3(-1.2 * cos(0.8 * uTime + 1.2), 0.6, -1.2 * sin(0.8 * uTime + 1.2));
    vec3 Lcol1 = vec3(0.60, 0.80, 1.00);  // cool

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Material parameters (constant for simplicity)
    vec3  albedo     = vec3(0.72, 0.72, 0.72);
    float ambientK   = 0.08;     // ambient term
    float specularK  = 0.5;      // overall specular strength
    float shininess  = 64.0;     // Blinn-Phong exponent
    vec3  F0         = vec3(0.04); // Fresnel reflectance at normal incidence (dielectric)

    // Accumulate contribution from both lights
    vec3 color = vec3(0.0);
    for (int i = 0; i < 2; ++i) {
        vec3 Lpos = (i == 0) ? Lpos0 : Lpos1;
        vec3 Lcol = (i == 0) ? Lcol0 : Lcol1;
        vec3 L    = Lpos - vWorldPos;
        float d   = length(L);
        L         = (d > 0.0) ? (L / d) : vec3(0.0, 1.0, 0.0);
        // Smooth distance attenuation
        float atten = 1.0 / (1.0 + 0.3 * d * d);

        // Diffuse (Lambert)
        float NdotL = max(dot(N, L), 0.0);
        vec3  diff  = albedo * NdotL;

        // Specular (Blinn-Phong with Schlick Fresnel)
        vec3 H      = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);
        vec3  F     = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
        float spec  = pow(NdotH, shininess) * specularK;

        // Rim light for a stylized edge glow
        float rim   = pow(1.0 - max(dot(N, V), 0.0), 2.0);
        vec3  rimCol = vec3(0.25, 0.30, 0.35) * rim;

        // Sum per-light contribution
        vec3 lightContrib = (diff + F * spec + rimCol) * Lcol * atten;
        color += lightContrib;
    }

    // Add ambient
    color += ambientK * albedo;

    // Gamma correct for display
    color = gammaCorrect(color);
    fColor = vec4(color, 1.0);
}
