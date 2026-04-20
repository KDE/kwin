#version 440
layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float damp;
    float phase;
    vec2 point;
    vec2 size;
};

layout(binding = 1) uniform sampler2D currentInput;
layout(binding = 2) uniform sampler2D previousFrame;

void main() {
    vec2 texelSize = 1.0 / size;

    vec4 curr_rgb = texture(currentInput, coord);
    vec4 prev_rgb = texture(previousFrame, coord);

    float new_r = (2.0 * prev_rgb.r - prev_rgb.g + phase * (
                    texture(previousFrame, coord - vec2(0.0, texelSize.y)).r +
                    texture(previousFrame, coord + vec2(0.0, texelSize.y)).r +
                    texture(previousFrame, coord - vec2(texelSize.x, 0.0)).r +
                    texture(previousFrame, coord + vec2(texelSize.x, 0.0)).r - 4.0 * prev_rgb.r)) * damp;

    if (curr_rgb.r > 0.4 && prev_rgb.b <= 0.4) {
        new_r += curr_rgb.r * 0.95;
    }

    if (prev_rgb.b > 0.1 && curr_rgb.r <= 0.1) {
        new_r -= prev_rgb.b * 0.5;
    }
    vec4 result = vec4(new_r, prev_rgb.r, curr_rgb.r, 1.0);

    fragColor = result * qt_Opacity;
}
