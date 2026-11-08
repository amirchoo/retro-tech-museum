const float PI = 3.141592653589793;

vec3 fresnelSchlick(float cosine, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0);
}

float ggxD(float noH, float alpha) {
    float a2 = alpha * alpha;
    float d = noH * noH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-20);
}

float ggxG1(float noV, float alpha) {
    if (noV <= 0.0) return 0.0;
    return 2.0 * noV / (noV + sqrt(alpha * alpha + (1.0 - alpha * alpha) * noV * noV));
}

vec3 evaluatePbr(vec3 color, float metallic, float roughness, vec3 n, vec3 v, vec3 l) {
    float noV = dot(n, v), noL = dot(n, l);
    if (noV <= 0.0 || noL <= 0.0 || dot(v + l, v + l) < 1e-12) return vec3(0.0);
    vec3 h = normalize(v + l);
    float alpha = max(roughness * roughness, 0.001);
    vec3 f = fresnelSchlick(max(dot(v, h), 0.0), mix(vec3(0.04), color, metallic));
    vec3 diffuse = (vec3(1.0) - f) * (1.0 - metallic) * color / PI;
    vec3 specular = f * ggxD(max(dot(n, h), 0.0), alpha) * ggxG1(noV, alpha) * ggxG1(noL, alpha) /
                    max(4.0 * noV * noL, 1e-12);
    return diffuse + specular;
}
