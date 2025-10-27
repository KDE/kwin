#include "color_helpers.glsl"

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

const mat3 toICtCp = mat3(
    0.5,  1.613769531250,   4.378173828125,
    0.5, -3.323486328125, -4.245605468750,
    0.0,  1.709716796875, -0.132568359375
);
const mat3 fromICtCp = mat3(
    1.0,               1.0,             1.0,
    0.00860903703793, -0.008609037037,  0.56031335710680,
    0.11102962500303, -0.111029625003, -0.32062717498732
);

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

    // apply tone mapping operation (modified Reinhart)
    float relativeLuminance = max(luminance / destinationReferenceLuminance, 0.0);
    float inputRange = maxTonemappingLuminance / destinationReferenceLuminance;
    float outputRange = maxDestinationLuminance / destinationReferenceLuminance;
    float v = (outputRange * (1.0 + inputRange) - inputRange) / pow(inputRange, 2.0);
    relativeLuminance = relativeLuminance * (1.0 + relativeLuminance * v) / (1.0 + relativeLuminance);
    luminance = relativeLuminance * destinationReferenceLuminance;

    // last, convert back to rgb
    ICtCp.r = singleLinearToPq(luminance / 10000.0);
    return (lmsToDestination * vec4(pqToLinear(fromICtCp * ICtCp), 1.0)).rgb * 10000.0;
}

vec4 sourceEncodingToNitsInDestinationColorspace(vec4 color) {
    color = encodingToNits(color, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    color.rgb = (colorimetryTransform * vec4(color.rgb, 1.0)).rgb;
    return vec4(doTonemapping(color.rgb), color.a);
}

vec4 nitsToDestinationEncoding(vec4 color) {
    return nitsToEncoding(color, destinationNamedTransferFunction, destinationTransferFunctionParams.x, destinationTransferFunctionParams.y);
}
