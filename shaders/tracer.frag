#version 410 core
#include "scene_access.glsl"
#include "pbr.glsl"
#include "lighting.glsl"

in vec2 vUV;
out vec4 fragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec2 uResolution;
uniform sampler2D uPrevious;
uniform sampler2DArray uBaseColorTextures;
uniform int uBaseColorLayerCount;
uniform int uPreviousSamples;
uniform int uFrame;
uniform int uSamplesPerFrame;
uniform int uMaxBounces;
uniform float uIndirectClamp;

uint rngState;

uint pcg(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float randomFloat() {
    return float(pcg(rngState) >> 8u) * (1.0 / 16777216.0);
}

mat3 frameFromNormal(vec3 n) {
    vec3 t = normalize(cross(abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0), n));
    return mat3(t, cross(n, t), n);
}

vec3 cosineHemisphere() {
    float r = sqrt(randomFloat());
    float phi = 2.0 * PI * randomFloat();
    return vec3(r * cos(phi), r * sin(phi), sqrt(max(1.0 - r * r, 0.0)));
}

// Heitz visible-normal sampling for isotropic GGX, in a local shading frame.
vec3 sampleGgxVndf(vec3 v, float alpha) {
    vec3 vh = normalize(vec3(alpha * v.xy, v.z));
    float lensq = dot(vh.xy, vh.xy);
    vec3 t1 = lensq > 1e-12 ? vec3(-vh.y, vh.x, 0) / sqrt(lensq) : vec3(1, 0, 0);
    vec3 t2 = cross(vh, t1);
    float r = sqrt(randomFloat()), phi = 2.0 * PI * randomFloat();
    float x = r * cos(phi), y = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    y = (1.0 - s) * sqrt(max(1.0 - x * x, 0.0)) + s * y;
    vec3 nh = x * t1 + y * t2 + sqrt(max(1.0 - x * x - y * y, 0.0)) * vh;
    return normalize(vec3(alpha * nh.xy, max(nh.z, 0.0)));
}

float specularProbability(RtmMaterial m) {
    return m.metallic >= 0.999 ? 1.0 : mix(0.25, 0.9, m.metallic);
}

float opaquePdf(RtmMaterial m, vec3 n, vec3 v, vec3 l) {
    float noV = dot(n, v), noL = dot(n, l);
    if (noV <= 0.0 || noL <= 0.0 || dot(v + l, v + l) < 1e-12) return 0.0;
    vec3 h = normalize(v + l);
    float alpha = max(m.roughness * m.roughness, 0.001);
    float specularPdf = ggxD(max(dot(n, h), 0.0), alpha) * ggxG1(noV, alpha) / max(4.0 * noV, 1e-12);
    return mix(noL / PI, specularPdf, specularProbability(m));
}

float powerWeight(float a, float b) {
    float scale = max(max(a, b), 1e-20);
    a /= scale;
    b /= scale;
    return a * a / max(a * a + b * b, 1e-20);
}

vec3 offsetOrigin(RtmSurface s, vec3 direction) {
    float epsilon = 1e-4 * max(1.0, max(max(abs(s.position.x), abs(s.position.y)), abs(s.position.z)));
    return s.position + s.geomNormal * (dot(direction, s.geomNormal) >= 0.0 ? epsilon : -epsilon);
}

vec3 limitIndirect(vec3 contribution, int bounce) {
    if (any(isnan(contribution)) || any(isinf(contribution))) return vec3(0);
    float peak = max(max(contribution.r, contribution.g), contribution.b);
    if (bounce > 0 && uIndirectClamp > 0.0 && peak > uIndirectClamp)
        contribution *= uIndirectClamp / peak;
    return contribution;
}

// The disk is an analytic emitter, independent of the packed mesh contract.
bool intersectLight(vec3 origin, vec3 direction, out float distance) {
    distance = RTM_INF;
    if (uLightRadius <= 1e-4) return false;
    float cosine = dot(direction, uLightDir);
    if (cosine >= -1e-6) return false;
    distance = dot(uLightPos - origin, uLightDir) / cosine;
    if (distance <= 1e-5) return false;
    vec3 radial = origin + direction * distance - uLightPos;
    return dot(radial, radial) <= uLightRadius * uLightRadius;
}

vec3 directLight(RtmSurface s, RtmMaterial m, vec3 v, bool useMis) {
    float opaqueWeight = 1.0 - m.transmission;
    if (opaqueWeight <= 0.0) return vec3(0);
    vec3 lightPoint = uLightPos;
    bool area = uLightRadius > 1e-4;
    float lightArea = PI * uLightRadius * uLightRadius;
    if (area) {
        float r = uLightRadius * sqrt(randomFloat());
        float phi = 2.0 * PI * randomFloat();
        lightPoint += frameFromNormal(uLightDir) * vec3(r * cos(phi), r * sin(phi), 0);
    }
    vec3 delta = lightPoint - s.position;
    float distance2 = dot(delta, delta);
    if (distance2 <= 1e-8) return vec3(0);
    float distance = sqrt(distance2);
    vec3 l = delta / distance;
    float noL = dot(s.normal, l);
    float cosLight = dot(uLightDir, -l);
    if (noL <= 0.0 || dot(s.geomNormal, l) <= 0.0 || cosLight <= 0.0) return vec3(0);
    float cone = lightCone(-l);
    if (cone <= 0.0) return vec3(0);

    vec3 origin = offsetOrigin(s, l);
    vec3 shadowDelta = lightPoint - origin;
    float shadowDistance = length(shadowDelta);
    // Glass occludes shadow rays here; transmission is handled by continuation
    // paths. Refractive caustics require a bidirectional technique, not NEE.
    RtmHit blocker = rtmIntersectScene(origin, shadowDelta / shadowDistance, shadowDistance - 1e-4, true);
    if (blocker.tri >= 0) return vec3(0);

    vec3 f = opaqueWeight * evaluatePbr(m.baseColor, m.metallic, m.roughness, s.normal, v, l);
    if (!area) return f * noL * uLightColor * cone * cosLight / distance2;
    float lightPdf = distance2 / (cosLight * lightArea);
    float bsdfPdf = opaqueWeight * opaquePdf(m, s.normal, v, l);
    vec3 radiance = uLightColor * cone / lightArea;
    return f * noL * radiance * (useMis ? powerWeight(lightPdf, bsdfPdf) : 1.0) / lightPdf;
}

vec3 tracePath(vec3 origin, vec3 direction) {
    vec3 radiance = vec3(0), throughput = vec3(1);
    float previousPdf = 0.0;
    bool previousDelta = true;
    vec3 previousPosition = origin;

    for (int bounce = 0; bounce < 16; ++bounce) {
        if (bounce >= uMaxBounces) break;
        RtmHit hit = rtmIntersectScene(origin, direction, RTM_INF, false);
        float lightDistance;
        if (intersectLight(origin, direction, lightDistance) && lightDistance < hit.t) {
            float area = PI * uLightRadius * uLightRadius;
            vec3 lightPoint = origin + direction * lightDistance;
            vec3 delta = lightPoint - previousPosition;
            float lightPdf = dot(delta, delta) / max(-dot(direction, uLightDir) * area, 1e-20);
            float weight = previousDelta ? 1.0 : powerWeight(previousPdf, lightPdf);
            radiance += limitIndirect(throughput * (uLightColor * lightCone(-direction) / area) * weight, bounce);
            break;
        }
        if (hit.tri < 0) {
            radiance += limitIndirect(throughput * vec3(uAmbient), bounce);
            break;
        }

        RtmSurface s = rtmSurface(origin, direction, hit);
        RtmMaterial m = rtmMaterial(s.material);
        m.baseColor = clamp(m.baseColor, 0.0, 1.0);
        if (m.baseColorTex >= 0 && m.baseColorTex < uBaseColorLayerCount)
            m.baseColor *= textureLod(uBaseColorTextures, vec3(s.uv, float(m.baseColorTex)), 0.0).rgb;
        m.metallic = clamp(m.metallic, 0.0, 1.0);
        m.roughness = clamp(m.roughness, 0.0, 1.0);
        m.transmission = clamp(m.transmission, 0.0, 1.0) * (1.0 - m.metallic);
        vec3 v = -direction;
        if (dot(s.normal, v) <= 1e-5) s.normal = s.geomNormal;
        radiance += limitIndirect(throughput * max(m.emission, vec3(0)), bounce);
        // The final vertex has no continuation sample, so NEE has weight 1.
        radiance += limitIndirect(throughput * directLight(s, m, v, bounce + 1 < uMaxBounces), bounce);
        if (bounce + 1 >= uMaxBounces) break;

        vec3 nextDirection;
        previousPosition = s.position;
        if (randomFloat() < m.transmission) {
            float ior = max(m.ior, 1.0001);
            float eta = s.frontFace ? 1.0 / ior : ior;
            float cosine = clamp(dot(v, s.geomNormal), 0.0, 1.0);
            float f0 = (ior - 1.0) / (ior + 1.0);
            f0 *= f0;
            float reflectance = f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
            vec3 transmitted = refract(direction, s.geomNormal, eta);
            if (dot(transmitted, transmitted) < 1e-12 || randomFloat() < reflectance) {
                nextDirection = reflect(direction, s.geomNormal);
            } else {
                nextDirection = normalize(transmitted);
                throughput *= sqrt(m.baseColor) * (eta * eta);
            }
            previousDelta = true;
            previousPdf = 0.0;
        } else {
            mat3 basis = frameFromNormal(s.normal);
            if (randomFloat() < specularProbability(m)) {
                vec3 localV = transpose(basis) * v;
                vec3 h = basis * sampleGgxVndf(localV, max(m.roughness * m.roughness, 0.001));
                nextDirection = reflect(-v, h);
            } else {
                nextDirection = basis * cosineHemisphere();
            }
            if (dot(s.geomNormal, nextDirection) <= 0.0) break;
            float pdf = opaquePdf(m, s.normal, v, nextDirection);
            if (pdf <= 1e-12) break;
            throughput *= evaluatePbr(m.baseColor, m.metallic, m.roughness, s.normal, v, nextDirection) *
                          max(dot(s.normal, nextDirection), 0.0) / pdf;
            previousPdf = (1.0 - m.transmission) * pdf;
            previousDelta = false;
        }
        if (any(isnan(throughput)) || any(isinf(throughput))) break;
        if (bounce >= 2) {
            float survival = clamp(max(max(throughput.r, throughput.g), throughput.b), 0.05, 0.95);
            if (randomFloat() >= survival) break;
            throughput /= survival;
        }
        direction = normalize(nextDirection);
        origin = offsetOrigin(s, direction);
    }
    if (any(isnan(radiance)) || any(isinf(radiance))) return vec3(0);
    return max(radiance, vec3(0));
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    uint pixelIndex = uint(pixel.x) + uint(pixel.y) * uint(uResolution.x);
    vec3 sum = vec3(0);
    for (int sampleIndex = 0; sampleIndex < 16; ++sampleIndex) {
        if (sampleIndex >= uSamplesPerFrame) break;
        rngState = pixelIndex * 1973u ^ uint(uPreviousSamples + sampleIndex) * 9277u ^ uint(uFrame) * 26699u ^ 0x68bc21ebu;
        pcg(rngState);
        vec2 jitter = vec2(randomFloat(), randomFloat());
        vec2 uv = (vec2(pixel) + jitter) / uResolution;
        vec4 farPoint = uInvViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
        vec3 direction = normalize(farPoint.xyz / farPoint.w - uCameraPos);
        sum += tracePath(uCameraPos, direction);
    }
    vec3 previous = uPreviousSamples > 0 ? texelFetch(uPrevious, pixel, 0).rgb : vec3(0);
    if (any(isnan(previous)) || any(isinf(previous))) previous = vec3(0);
    float total = float(uPreviousSamples + uSamplesPerFrame);
    fragColor = vec4(previous + (sum - previous * float(uSamplesPerFrame)) / total, 1.0);
}
