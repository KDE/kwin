#include "colormanagement_helpers.glsl"

const int PipelineOperationTransferFunction = 0;
const int PipelineOperationInverseTransferFunction = 1;
const int PipelineOperationMatrix = 2;
const int PipelineOperationToneMapper = 3;
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
        }
    }
    return outputColor;
}
