#version 450

layout(set = 0, binding = 0) writeonly uniform image2D outputImage;
layout(set = 1, binding = 0) uniform sampler2D inputImage;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main()
{
    if (any(greaterThan(gl_GlobalInvocationID.xy, imageSize(outputImage)))) {
        return;
    }
    imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), texture(inputImage, gl_GlobalInvocationID.xy + vec2(0.5)));
}
