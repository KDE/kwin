#include "colormanagement_helpers.glsl"

precision highp float;
precision highp sampler2D;
precision highp sampler3D;

const int PipelineOperationTransferFunction = 0;
const int PipelineOperationInverseTransferFunction = 1;
const int PipelineOperationMatrix = 2;
const int PipelineOperationToneMapper = 3;
const int PipelineOperationLut1D = 4;
const int PipelineOperationLut3D = 5;
struct PipelineOperation
{
    int type;
    int typeIndex;
};
uniform PipelineOperation pipelineOperations[20];
uniform int pipelineOperationCount;

struct TransferFunction
{
    int type;
    float minLuminance;
    float maxMinusMinLuminance;
};
uniform TransferFunction pipelineTransferFunctions[10];

uniform mat4 pipelineMatrices[10];

struct ToneMapper
{
    float maxDestinationLuminance;
    float inputRange;
    float referenceDimming;
    float outputReferenceLuminance;
};
uniform ToneMapper pipelineToneMapper;

struct Lut1D
{
    sampler2D sampler;
    int size;
};
uniform Lut1D pipelineLut1Ds[4];

struct Lut3D
{
    sampler3D sampler;
    ivec3 size;
};
uniform Lut3D pipelineLut3D;

vec3 sample1DLut(in vec3 srcColor, in Lut1D lut) {
    float lutOffset = 0.5 / float(lut.size);
    float lutScale = 1.0 - lutOffset * 2.0;
    float lutR = texture(lut.sampler, vec2(lutOffset + srcColor.r * lutScale, 0.5)).r;
    float lutG = texture(lut.sampler, vec2(lutOffset + srcColor.g * lutScale, 0.5)).g;
    float lutB = texture(lut.sampler, vec2(lutOffset + srcColor.b * lutScale, 0.5)).b;
    return vec3(lutR, lutG, lutB);
}

vec3 sample3DLut(in vec3 srcColor, in Lut3D lut) {
    vec3 lutOffset = vec3(0.5) / vec3(lut.size);
    vec3 lutScale = vec3(1) - lutOffset * 2.0;
    return texture(lut.sampler, lutOffset + srcColor.rgb * lutScale).rgb;
}

vec4 applyTonemapper(vec4 color, ToneMapper mapper) {
    float luminance = singlePqToLinear(color.r) * 10000.0;
    // keep it linear up to the reference luminance
    float low = min(luminance * mapper.referenceDimming, mapper.outputReferenceLuminance);
    // and apply a nonlinear curve above, to reduce the luminance without completely removing differences
    float relativeHighlight = clamp((luminance * mapper.referenceDimming / mapper.outputReferenceLuminance - 1.0) / (mapper.inputRange - 1.0), 0.0, 1.0);
    const float e = 2.718281828459045;
    float high = log(relativeHighlight * (e - 1.0) + 1.0) * (mapper.maxDestinationLuminance - mapper.outputReferenceLuminance);
    return vec4(singleLinearToPq((low + high) / 10000.0), color.g, color.b, color.a);
}

vec4 applyColorPipeline(vec4 inputColor)
{
    vec4 outputColor = inputColor;
    for (int i = 0; i < pipelineOperationCount; i++) {
        int operationIndex = pipelineOperations[i].typeIndex;
        switch (pipelineOperations[i].type) {
        case PipelineOperationTransferFunction:
            outputColor = encodingToNits(outputColor, pipelineTransferFunctions[operationIndex].type, pipelineTransferFunctions[operationIndex].minLuminance, pipelineTransferFunctions[operationIndex].maxMinusMinLuminance);
            break;
        case PipelineOperationInverseTransferFunction:
            outputColor = nitsToEncoding(outputColor, pipelineTransferFunctions[operationIndex].type, pipelineTransferFunctions[operationIndex].minLuminance, pipelineTransferFunctions[operationIndex].maxMinusMinLuminance);
            break;
        case PipelineOperationMatrix:
            outputColor = pipelineMatrices[operationIndex] * outputColor;
            break;
        case PipelineOperationToneMapper:
            outputColor = applyTonemapper(outputColor, pipelineToneMapper);
            break;
        case PipelineOperationLut1D:
            outputColor.rgb = sample1DLut(outputColor.rgb, pipelineLut1Ds[operationIndex]);
            break;
        case PipelineOperationLut3D:
            outputColor.rgb = sample3DLut(outputColor.rgb, pipelineLut3D);
            break;
        }
    }
    return outputColor;
}
