#version 450

layout(set = 0, binding = 0) writeonly uniform image2D outputImage;

layout(std140, push_constant) uniform pc {
    uint itemCount;
};

struct Item {
    vec4 rect;
    vec4 color;
};

layout(std140, set = 1, binding = 0) readonly buffer scene {
    Item items[];
};

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main()
{
    vec4 result = vec4(0, 0, 0, 0);
    for (uint i = 0; i < itemCount; i++) {
        bool intersection = gl_GlobalInvocationID.x >= items[i].rect.x
            && gl_GlobalInvocationID.x < items[i].rect.z
            && gl_GlobalInvocationID.y >= items[i].rect.y
            && gl_GlobalInvocationID.y < items[i].rect.w;
        if (intersection) {
            result = items[i].color;
            break;
        }
    }
    imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), result);
}
