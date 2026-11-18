#version 410 core
// Constant-material PBR baseline; shadow/normal maps remain the raster handoff.
#include "pbr.glsl"
#include "lighting.glsl"
#include "tonemap.glsl"

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform vec3 uCameraPos;
uniform float uExposure;

uniform vec3 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmission;

out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    if (dot(n, V) < 0.0) n = -n;
    vec3 f = evaluatePbr(uBaseColor, uMetallic, uRoughness, n, V, L);
    vec3 color = uBaseColor * uAmbient + f * max(dot(n, L), 0.0) * pointLightIrradiance(vWorldPos) + uEmission;
    fragColor = vec4(displayColor(color, uExposure), 1.0);
}
