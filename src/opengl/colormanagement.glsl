const int sRGB_EOTF = 0;
const int linear_EOTF = 1;
const int PQ_EOTF = 2;
const int scRGB_EOTF = 3;
const int gamma22_EOTF = 4;

uniform mat3 colorimetryTransform;
uniform int sourceNamedTransferFunction;
uniform int destinationNamedTransferFunction;
uniform float sdrBrightness;// in nits
uniform float maxHdrBrightness; // in nits

vec3 nitsToPq(vec3 nits) {
    vec3 normalized = clamp(nits / 10000.0, vec3(0), vec3(1));
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    vec3 powed = pow(normalized, vec3(m1));
    vec3 num = vec3(c1) + c2 * powed;
    vec3 denum = vec3(1.0) + c3 * powed;
    return pow(num / denum, vec3(m2));
}
vec3 pqToNits(vec3 pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    vec3 powed = pow(pq, vec3(m2_inv));
    vec3 num = max(powed - c1, vec3(0.0));
    vec3 den = c2 - c3 * powed;
    return 10000.0 * pow(num / den, vec3(m1_inv));
}
vec3 srgbToLinear(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.04045f));
    vec3 loPart = color / 12.92f;
    vec3 hiPart = pow((color + 0.055f) / 1.055f, vec3(12.0f / 5.0f));
#if __VERSION__ >= 130
    return mix(hiPart, loPart, isLow);
#else
    return mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif
}

vec3 linearToSrgb(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.0031308f));
    vec3 loPart = color * 12.92f;
    vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
#if __VERSION__ >= 130
    return mix(hiPart, loPart, isLow);
#else
    return mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif
}

vec3 doTonemapping(vec3 color, float maxBrightness) {
    // TODO do something better here
    return clamp(color, vec3(0.0), vec3(maxBrightness));
}

vec4 encodingToNits(vec4 color, int sourceTransferFunction) {
    if (sourceTransferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = sdrBrightness * srgbToLinear(color.rgb);
        color.rgb *= color.a;
    } else if (sourceTransferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pqToNits(color.rgb);
        color.rgb *= color.a;
    } else if (sourceTransferFunction == scRGB_EOTF) {
        color.rgb *= 80.0;
    } else if (sourceTransferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = sdrBrightness * pow(color.rgb, vec3(2.2));
        color.rgb *= color.a;
    }
    return color;
}

vec4 sourceEncodingToNitsInDestinationColorspace(vec4 color) {
    color = encodingToNits(color, sourceNamedTransferFunction);
    return vec4(doTonemapping(colorimetryTransform * color.rgb, maxHdrBrightness), color.a);
}

vec4 nitsToEncoding(vec4 color, int destinationTransferFunction) {
    if (destinationTransferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = linearToSrgb(doTonemapping(color.rgb, sdrBrightness) / sdrBrightness);
        color.rgb *= color.a;
    } else if (destinationTransferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = nitsToPq(color.rgb);
        color.rgb *= color.a;
    } else if (destinationTransferFunction == scRGB_EOTF) {
        color.rgb /= 80.0;
    } else if (destinationTransferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pow(color.rgb / sdrBrightness, vec3(1.0 / 2.2));
        color.rgb *= color.a;
    }
    return color;
}

vec4 nitsToDestinationEncoding(vec4 color) {
    return nitsToEncoding(color, destinationNamedTransferFunction);
}
