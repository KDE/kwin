uniform sampler2D sampler;
uniform float u_alphaProgress;

varying vec2 texcoord0;

void main() {
    gl_FragColor = texture2D(sampler, texcoord0, 1.75);
    gl_FragColor.a = u_alphaProgress;
}
