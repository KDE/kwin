const int sRGB_EOTF = 0;
const int linear_EOTF = 1;
const int PQ_EOTF = 2;
const int gamma22_EOTF = 3;
const int BT1886_EOTF = 4;
const int HLG_EOTF = 5;

uniform mat4 colorimetryTransform;

uniform int sourceNamedTransferFunction;
/**
 * Parameters specific to the transfer function
 */
uniform vec2 sourceTransferFunctionParams;

uniform int destinationNamedTransferFunction;
/**
 * Parameters specific to the transfer function
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

vec3 hlgToNits(vec3 color, float beta, float maxLuminance)
{
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;

    // first, apply the black level adjustment
    vec3 adjusted = max(vec3(0.0), vec3(1.0 - beta) * color + vec3(beta));

    // then the inverse OETF
    vec3 loPart = pow(adjusted, vec3(2.0)) / 3.0;
    vec3 hiPart = (exp((adjusted - vec3(c)) / vec3(a)) + vec3(b)) / vec3(12.0);

    bvec3 isLow = lessThanEqual(adjusted, vec3(0.5));
#if __VERSION__ >= 130
    vec3 nits = maxLuminance * mix(hiPart, loPart, isLow);
#else
    vec3 nits = maxLuminance * mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif

    // then the OOTF
    float Y = dot(vec3(0.2627, 0.6780, 0.0593), nits);
    // TODO in the next shader API break, pre-compute gamma and alpha
    float gamma = 1.2 + 0.42 * log(maxLuminance / 1000.0) / log(10.0);
    float alpha = 1.0 / pow(maxLuminance, gamma - 1.0);
    float factor = alpha * pow(Y, gamma - 1.0);
    return factor * nits;
}

vec3 nitsToHLG(vec3 nits, float beta, float maxLuminance)
{
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;

    // first apply the inverse OOTF
    float Y = dot(vec3(0.2627, 0.6780, 0.0593), nits);
    float gamma = 1.2 + 0.42 * log(maxLuminance / 1000.0) / log(10.0);
    float alpha = 1.0 / pow(maxLuminance, gamma - 1.0);

    float ootfLum = pow(max(Y / alpha, 0.0001), (1.0 - gamma) / gamma);
    float factor = ootfLum / alpha;

    // then the OETF
    vec3 E = clamp(nits * vec3(factor / maxLuminance), 0.0, 1.0);

    vec3 loPart = sqrt(3.0 * E);
    vec3 hiPart = a * log(12.0 * E - b) + c;

    bvec3 isLow = lessThanEqual(E, vec3(1.0 / 12.0));
#if __VERSION__ >= 130
    vec3 almostHLG = mix(hiPart, loPart, isLow);
#else
    vec3 almostHLG = mix(hiPart, loPart, vec3(isLow.r ? 1.0 : 0.0, isLow.g ? 1.0 : 0.0, isLow.b ? 1.0 : 0.0));
#endif

    // then the inverse black level adjustment
    return (almostHLG - vec3(beta)) / (1.0 - beta);
}

const mat3 toICtCp = mat3(
    0.5,  1.613769531250,   4.378173828125,
    0.5, -3.323486328125, -4.245605468750,
    0.0,  1.709716796875, -0.132568359375
);
const mat3 fromICtCp = mat3(
    1.0,               1.0,               1.0,
    0.00860903703793, -0.00860903703793,  0.56003133571068,
    0.11102962500303, -0.11102962500303, -0.32062717498732
);

vec3 doTonemapping(vec3 color) {
    if (maxTonemappingLuminance < maxDestinationLuminance * 1.01) {
        // clipping is enough
        return clamp(color.rgb, vec3(0.0), vec3(maxDestinationLuminance));
    }

    // convert to ICtCp, to properly split luminance and color
    // intensity is PQ-encoded luminance
    vec3 lms = (destinationToLMS * vec4(color, 1.0)).rgb;
    vec3 lms_PQ = linearToPq(lms / 10000.0);
    vec3 ICtCp = toICtCp * lms_PQ;
    float luminance = singlePqToLinear(ICtCp.r) * 10000.0;

    // apply tone mapping operation (modified Reinhart)
    float relativeLuminance = max(luminance / destinationReferenceLuminance, 0.0);
    float inputRange = maxTonemappingLuminance / destinationReferenceLuminance;
    float outputRange = maxDestinationLuminance / destinationReferenceLuminance;
    float v = (outputRange * (1.0 + inputRange) - inputRange) / pow(inputRange, 2.0);
    relativeLuminance = relativeLuminance * (1.0 + relativeLuminance * v) / (1.0 + relativeLuminance);
    luminance = relativeLuminance * destinationReferenceLuminance;

    // convert back to rgb
    ICtCp.r = singleLinearToPq(luminance / 10000.0);
    color = (lmsToDestination * vec4(pqToLinear(fromICtCp * ICtCp), 1.0)).rgb * 10000.0;
    // and clip, to ensure out-of-gamut values are clipped to the correct white point
    return clamp(color, vec3(0.0), vec3(maxDestinationLuminance));
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
    } else if (sourceTransferFunction == BT1886_EOTF) {
        color.rgb /= max(color.a, 0.001);
        // for bt1886, luminanceScale = a, luminanceOffset = b
        color.rgb = luminanceScale * pow(max(color.rgb + vec3(luminanceOffset), vec3(0.0)), vec3(2.4));
        color.rgb *= color.a;
    } else if (sourceTransferFunction == HLG_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = hlgToNits(color.rgb, luminanceOffset, luminanceScale);
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
    } else if (destinationTransferFunction == BT1886_EOTF) {
        color.rgb /= max(color.a, 0.001);
        // for bt1886, luminanceScale = a, luminanceOffset = b
        color.rgb = pow(color.rgb / luminanceScale, vec3(1.0 / 2.4)) - vec3(luminanceOffset);
        color.rgb *= color.a;
    } else if (destinationTransferFunction == HLG_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = nitsToHLG(color.rgb, luminanceOffset, luminanceScale);
        color.rgb *= color.a;
    }
    return color;
}

vec4 nitsToDestinationEncoding(vec4 color) {
    return nitsToEncoding(color, destinationNamedTransferFunction, destinationTransferFunctionParams.x, destinationTransferFunctionParams.y);
}
