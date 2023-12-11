uniform float saturation;
uniform vec3 primaryBrightness;

vec4 adjustSaturation(vec4 color) {
    // this calculates the Y component of the XYZ color representation for the color,
    // which roughly corresponds to the brightness of the RGB tuple
    float Y = dot(color.rgb, primaryBrightness);
    return vec4(mix(vec3(Y), color.rgb, saturation), color.a);
}
