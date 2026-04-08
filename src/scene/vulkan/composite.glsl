#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_NV_compute_shader_derivatives : require

layout(set = 0, binding = 0) writeonly uniform image2D outputImage;

layout(std140, push_constant) uniform pc {
    uint itemCount;
};

struct Item {
    vec4 rect;
    vec4 color;
    int textureIndex;
    float outlineWidth;
    int padding;
    int padding2;
    vec4 box;
    vec4 cornerRadius;
};

layout(std140, set = 1, binding = 0) readonly buffer scene {
    Item items[];
};
layout(set = 2, binding = 0) uniform sampler2D textures[];

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 doBlending(in vec4 color, in vec4 background)
{
    vec3 ret = color.rgb + background.rgb * (1 - color.a);
    return vec4(ret, color.a);
}

float sdfRoundedBox(in vec2 position, in vec2 center, in vec2 extents, in vec4 radius) {
    vec2 p = position - center;
    float r = p.x > 0.0
        ? (p.y < 0.0 ? radius.y : radius.w)
        : (p.y < 0.0 ? radius.x : radius.z);
    vec2 q = abs(p) - extents + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float sdfSubtract(float f1, float f2) {
    return max(f1, -f2);
}

float doOutline(in vec2 position0, in vec4 box, in vec4 cornerRadius, in float thickness)
{
    float inner = sdfRoundedBox(position0, box.xy, box.zw, cornerRadius);
    float outer = sdfRoundedBox(position0, box.xy, box.zw + vec2(thickness), cornerRadius + vec4(thickness));
    float f = sdfSubtract(outer, inner);
    float df = fwidth(f);
    return 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
}

float doRoundedCorners(in vec2 position0, in vec4 box, in vec4 cornerRadius)
{
    float f = sdfRoundedBox(position0, box.xy, box.zw, cornerRadius);
    float df = fwidth(f);
    return 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
}

void main()
{
    vec4 result = vec4(0.0);
    for (uint i = 0; i < itemCount; i++) {
        vec4 color;
        if (items[i].outlineWidth > 0) {
            color = items[i].color * doOutline(items[i].rect.xy, items[i].box, items[i].cornerRadius, items[i].outlineWidth);
        } else {
            float factor = doRoundedCorners(gl_GlobalInvocationID.xy, items[i].box, items[i].cornerRadius);
            vec2 pixelUV = gl_GlobalInvocationID.xy - items[i].rect.xy;
            vec2 uv = pixelUV / items[i].rect.zw;
            if (factor > 0) {
                vec4 color;
                if (items[i].textureIndex < 0) {
                    color = items[i].color;
                } else {
                    color = texture(textures[nonuniformEXT(items[i].textureIndex)], uv);
                }
                result = doBlending(color * factor, result);
            }
        }
    }
    imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), result);
}
