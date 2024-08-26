const int sRGB_EOTF = 0;
const int linear_EOTF = 1;
const int PQ_EOTF = 2;
const int gamma22_EOTF = 3;

uniform mat4 colorimetryTransform;

uniform int sourceNamedTransferFunction;
/**
 * x: min luminance
 * y: max luminance - min luminance
 */
uniform vec2 sourceTransferFunctionParams;

uniform int destinationNamedTransferFunction;
/**
 * x: min luminance
 * y: max luminance - min luminance
 */
uniform vec2 destinationTransferFunctionParams;

// in nits
uniform float sourceReferenceLuminance;
uniform float maxTonemappingLuminance;
uniform float destinationReferenceLuminance;
uniform float maxDestinationLuminance;

uniform mat4 destinationToLMS;
uniform mat4 lmsToDestination;

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

const mat3 toICtCp = transpose(mat3(
    2048.0 / 4096.0,   2048.0 / 4096.0,   0.0,
    6610.0 / 4096.0,  -13613.0 / 4096.0,  7003.0 / 4096.0,
    17933.0 / 4096.0, -17390.0 / 4096.0, -543.0 / 4096.0
));
const mat3 fromICtCp = inverse(toICtCp);

vec3 doTonemapping(vec3 color) {
    if (maxTonemappingLuminance < maxDestinationLuminance * 1.01) {
        // clipping is enough
        return clamp(color.rgb, vec3(0.0), vec3(maxDestinationLuminance));
    }

    // first, convert to ICtCp, to properly split luminance and color
    // intensity is PQ-encoded luminance
    vec3 lms = (destinationToLMS * vec4(color, 1.0)).rgb;
    vec3 lms_PQ = linearToPq(lms / 10000.0);
    vec3 ICtCp = toICtCp * lms_PQ;
    float luminance = singlePqToLinear(ICtCp.r) * 10000.0;

    // if the reference is too close to the maximum luminance, reduce it to get up to 50% headroom
    float inputRange = maxTonemappingLuminance / destinationReferenceLuminance;
    float outputRange = maxDestinationLuminance / destinationReferenceLuminance;
    float addedRange = min(inputRange, clamp(1.5 / outputRange, 1.0, 1.5));
    float outputReferenceLuminance = destinationReferenceLuminance / addedRange;

    // keep it linear up to the reference luminance
    float low = min(luminance / addedRange, outputReferenceLuminance);
    // and apply a nonlinear curve above, to reduce the luminance without completely removing differences
    float relativeHighlight = clamp((luminance / destinationReferenceLuminance - 1.0) / (inputRange - 1.0), 0.0, 1.0);
    const float e = 2.718281828459045;
    float high = log(relativeHighlight * (e - 1.0) + 1.0) * (maxDestinationLuminance - outputReferenceLuminance);
    luminance = low + high;

    // last, convert back to rgb
    ICtCp.r = singleLinearToPq(luminance / 10000.0);
    return (lmsToDestination * vec4(pqToLinear(fromICtCp * ICtCp), 1.0)).rgb * 10000.0;
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

vec4 sourceEncodingToNitsInDestinationColorspace(vec4 color) {
    color = encodingToNits(color, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    color.rgb = (colorimetryTransform * vec4(color.rgb, 1.0)).rgb;
    return vec4(doTonemapping(color.rgb), color.a);
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

vec4 nitsToDestinationEncoding(vec4 color) {
    return nitsToEncoding(color, destinationNamedTransferFunction, destinationTransferFunctionParams.x, destinationTransferFunctionParams.y);
}
