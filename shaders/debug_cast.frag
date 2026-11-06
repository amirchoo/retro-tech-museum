#version 410 core
// Debug ray caster: 1 primary ray + 1 shadow ray per pixel through scene_access.glsl.
// Proves the GPU scene layout + BVH traversal end to end, and is the starting point
// for 하직's path tracer (copy the ray setup; replace shading with path tracing).
#include "scene_access.glsl"

in vec2 vUV;
out vec4 fragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uLightPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uCosInner;
uniform float uCosOuter;
uniform float uAmbient;
uniform int uView;  // 0 = shaded, 1 = normals, 2 = instance id

vec3 hashColor(int i) {
    return fract(sin(vec3(float(i) * 12.9898, float(i) * 78.233, float(i) * 37.719)) * 43758.5453);
}

void main() {
    vec4 far = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - uCameraPos);

    RtmHit hit = rtmIntersectScene(uCameraPos, dir, RTM_INF, false);
    if (hit.t >= RTM_INF) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    RtmSurface s = rtmSurface(uCameraPos, dir, hit);
    if (uView == 1) { fragColor = vec4(s.normal * 0.5 + 0.5, 1.0); return; }
    if (uView == 2) { fragColor = vec4(hashColor(hit.instance + 1), 1.0); return; }

    RtmMaterial m = rtmMaterial(s.material);
    vec3 toLight = uLightPos - s.position;
    float dist = length(toLight);
    vec3 L = toLight / dist;
    vec3 V = -dir;
    vec3 H = normalize(L + V);

    float spot = smoothstep(uCosOuter, uCosInner, dot(-L, uLightDir));
    float atten = spot / (1.0 + 0.09 * dist + 0.032 * dist * dist);
    float visible = 1.0;
    if (atten > 0.0) {
        RtmHit sh = rtmIntersectScene(s.position + s.geomNormal * 1e-3, L, dist - 2e-3, true);
        visible = sh.t < RTM_INF ? 0.0 : 1.0;
    }

    vec3 diffuse = m.baseColor * max(dot(s.normal, L), 0.0);
    float shininess = mix(128.0, 4.0, m.roughness);
    vec3 specular = mix(vec3(0.04), m.baseColor, m.metallic) * pow(max(dot(s.normal, H), 0.0), shininess);
    vec3 color = m.baseColor * uAmbient + (diffuse + specular) * uLightColor * atten * visible + m.emission;

    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
