#version 410 core
// Debug ray caster: 1 primary ray + 1 shadow ray per pixel through scene_access.glsl.
// Proves the GPU scene layout + BVH traversal end to end, and is the starting point
// for 하직's path tracer (copy the ray setup; replace shading with path tracing).
#include "scene_access.glsl"
#include "pbr.glsl"
#include "lighting.glsl"
#include "tonemap.glsl"

in vec2 vUV;
out vec4 fragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform float uExposure;
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
    float visible = 1.0;
    if (lightCone(-L) > 0.0) {
        RtmHit sh = rtmIntersectScene(s.position + s.geomNormal * 1e-3, L, dist - 2e-3, true);
        visible = sh.t < RTM_INF ? 0.0 : 1.0;
    }

    vec3 f = evaluatePbr(m.baseColor, m.metallic, m.roughness, s.normal, V, L);
    vec3 color = m.baseColor * uAmbient + f * max(dot(s.normal, L), 0.0) * pointLightIrradiance(s.position) * visible + m.emission;
    fragColor = vec4(displayColor(color, uExposure), 1.0);
}
