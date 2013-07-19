#version 140

uniform vec2 u_center;
uniform float u_radius;
uniform float u_progress;

out vec4 fragColor;

void main() {
    float d = smoothstep(0, u_radius, distance(gl_FragCoord.xy, u_center));
    fragColor = vec4(0.0, 0.0, 0.0, d * u_progress);
}
