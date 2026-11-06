#version 410 core
// Placeholder shading for the skeleton: one ceiling spotlight + ambient, Blinn-Phong.
// The real PBR raster path (src/raster) replaces this.

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform vec3 uCameraPos;
uniform vec3 uLightPos;
uniform vec3 uLightDir;      // spotlight direction (normalised)
uniform vec3 uLightColor;
uniform float uCosInner;
uniform float uCosOuter;
uniform float uAmbient;

out vec4 fragColor;

vec3 albedo(vec3 p, vec3 n) {
    if (n.y > 0.9 && p.y < 0.01) {                       // floor: 1 m checker
        float c = mod(floor(p.x) + floor(p.z), 2.0);
        return mix(vec3(0.20, 0.17, 0.14), vec3(0.32, 0.27, 0.22), c);
    }
    if (p.y > 0.01 && p.y < 1.01 && abs(p.z + 2.5) < 0.41)
        return vec3(0.85, 0.82, 0.75);                    // plinths
    return vec3(0.62, 0.56, 0.48);                        // walls / ceiling
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float dist = length(uLightPos - vWorldPos);
    float spot = smoothstep(uCosOuter, uCosInner, dot(-L, uLightDir));
    float atten = spot / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    vec3 base = albedo(vWorldPos, n);
    vec3 diffuse = base * max(dot(n, L), 0.0);
    vec3 specular = vec3(0.15) * pow(max(dot(n, H), 0.0), 32.0);
    vec3 color = base * uAmbient + (diffuse + specular) * uLightColor * atten;

    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);   // gamma
}
