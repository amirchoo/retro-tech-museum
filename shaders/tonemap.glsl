vec3 displayColor(vec3 color, float exposure) {
    color = max(color * exposure, vec3(0.0));
    color = clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
    return pow(color, vec3(1.0 / 2.2));
}
