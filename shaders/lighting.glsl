uniform vec3 uLightPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uCosInner;
uniform float uCosOuter;
uniform float uLightRadius;
uniform float uAmbient;

float lightCone(vec3 fromLight) {
    return smoothstep(uCosOuter, uCosInner, dot(fromLight, uLightDir));
}

vec3 pointLightIrradiance(vec3 position) {
    vec3 fromLight = position - uLightPos;
    float distance2 = max(dot(fromLight, fromLight), 1e-6);
    fromLight *= inversesqrt(distance2);
    return uLightColor * lightCone(fromLight) * max(dot(fromLight, uLightDir), 0.0) / distance2;
}
