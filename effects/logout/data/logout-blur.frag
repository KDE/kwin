uniform sampler2D sampler;
uniform float u_alphaProgress;

varying vec2 varyingTexCoords;

void main() {
    gl_FragColor = texture2D(sampler, varyingTexCoords, 1.75);
    gl_FragColor.a = u_alphaProgress;
}
