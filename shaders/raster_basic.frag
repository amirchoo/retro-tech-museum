#version 410 core
// Placeholder raster shading: one spotlight + ambient, Blinn-Phong, material constants only.
// Same lighting model as debug_cast.frag (minus shadows), so the two modes can be compared.
// 알리's PBR raster path replaces this.

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform vec3 uCameraPos;
uniform vec3 uLightPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uCosInner;
uniform float uCosOuter;
uniform float uAmbient;

uniform vec3 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmission;

out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float dist = length(uLightPos - vWorldPos);
    float spot = smoothstep(uCosOuter, uCosInner, dot(-L, uLightDir));
    float atten = spot / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    vec3 diffuse = uBaseColor * max(dot(n, L), 0.0);
    float shininess = mix(128.0, 4.0, uRoughness);
    vec3 specular = mix(vec3(0.04), uBaseColor, uMetallic) * pow(max(dot(n, H), 0.0), shininess);
    vec3 color = uBaseColor * uAmbient + (diffuse + specular) * uLightColor * atten + uEmission;

    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
