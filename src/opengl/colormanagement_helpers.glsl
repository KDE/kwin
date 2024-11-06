const int sRGB_EOTF = 0;
const int linear_EOTF = 1;
const int PQ_EOTF = 2;
const int gamma22_EOTF = 3;

vec3 linearToPq(vec3 linear) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    vec3 powed = pow(clamp(linear, vec3(0), vec3(1)), vec3(m1));
    vec3 num = vec3(c1) + c2 * powed;
    vec3 denum = vec3(1.0) + c3 * powed;
    return pow(num / denum, vec3(m2));
}
vec3 pqToLinear(vec3 pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    vec3 powed = pow(clamp(pq, vec3(0.0), vec3(1.0)), vec3(m2_inv));
    vec3 num = max(powed - c1, vec3(0.0));
    vec3 den = c2 - c3 * powed;
    return pow(num / den, vec3(m1_inv));
}
float singleLinearToPq(float linear) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    float powed = pow(clamp(linear, 0.0, 1.0), m1);
    float num = c1 + c2 * powed;
    float denum = 1.0 + c3 * powed;
    return pow(num / denum, m2);
}
float singlePqToLinear(float pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    float powed = pow(clamp(pq, 0.0, 1.0), m2_inv);
    float num = max(powed - c1, 0.0);
    float den = c2 - c3 * powed;
    return pow(num / den, m1_inv);
}
vec3 srgbToLinear(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.04045));
    vec3 loPart = color / 12.92;
    vec3 hiPart = pow((color + 0.055) / 1.055, vec3(12.0 / 5.0));
#if __VERSION__ >= 130
    return mix(hiPart, loPart, isLow);
#else
    return mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif
}

vec3 linearToSrgb(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.0031308));
    vec3 loPart = color * 12.92;
    vec3 hiPart = pow(color, vec3(5.0 / 12.0)) * 1.055 - 0.055;
#if __VERSION__ >= 130
    return mix(hiPart, loPart, isLow);
#else
    return mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif
}

vec4 encodingToNits(vec4 color, int sourceTransferFunction, float luminanceOffset, float luminanceScale) {
    if (sourceTransferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = srgbToLinear(color.rgb) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    } else if (sourceTransferFunction == linear_EOTF) {
        color.rgb = color.rgb * luminanceScale + vec3(luminanceOffset);
    } else if (sourceTransferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pqToLinear(color.rgb) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    } else if (sourceTransferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(2.2)) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    }
    return color;
}

vec4 nitsToEncoding(vec4 color, int destinationTransferFunction, float luminanceOffset, float luminanceScale) {
    if (destinationTransferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = linearToSrgb((color.rgb - vec3(luminanceOffset)) / luminanceScale);
        color.rgb *= color.a;
    } else if (destinationTransferFunction == linear_EOTF) {
        color.rgb = (color.rgb - vec3(luminanceOffset)) / luminanceScale;
    } else if (destinationTransferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = linearToPq((color.rgb - vec3(luminanceOffset)) / luminanceScale);
        color.rgb *= color.a;
    } else if (destinationTransferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pow(max((color.rgb - vec3(luminanceOffset)) / luminanceScale, vec3(0.0)), vec3(1.0 / 2.2));
        color.rgb *= color.a;
    }
    return color;
}
