#version 410 core
#include "tonemap.glsl"

in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uAccumulation;
uniform float uExposure;
uniform float uDivider;

void main() {
    if (vUV.x < uDivider) discard;
    fragColor = vec4(displayColor(texture(uAccumulation, vUV).rgb, uExposure), 1.0);
}
