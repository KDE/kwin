#version 440
layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
};
layout(binding = 1) uniform sampler2D simulation;
layout(binding = 2) uniform sampler2D src;
void main() {
    vec4 simData = texture(simulation, coord);
    //simData = min(simData, vec4(0.8,0.8,0.8, 1.0));
    float factor = 1.0 - (simData.r - simData.g);// * 0.2;
    factor=max(0.99, factor);
    vec2 scaledCoord = coord * factor + vec2(0.5 - factor * 0.5);
    vec4 tex = texture(src, scaledCoord);
    tex = mix(tex, color, min(simData.r, 0.4));

    fragColor = tex * qt_Opacity;
}
