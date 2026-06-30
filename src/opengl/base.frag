#version 140

#if TRAIT_MAP_TEXTURE || TRAIT_MAP_MULTI_PLANE_TEXTURE
uniform sampler2D sampler;
in vec2 texcoord0;
#endif

#if TRAIT_MAP_MULTI_PLANE_TEXTURE
uniform sampler2D sampler1;
#endif

#if TRAIT_MAP_EXTERNAL_TEXTURE
#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES sampler;
in vec2 texcoord0;
#endif

#if TRAIT_UNIFORM_COLOR
uniform vec4 geometryColor;
#endif

#if TRAIT_BORDER || TRAIT_ROUNDED_CORNERS
#include "sdf.glsl"
uniform vec4 box;
uniform vec4 cornerRadius;
in vec2 position0;
#endif

#if TRAIT_BORDER
uniform vec4 geometryColor;
uniform int thickness;
#endif

#if TRAIT_YUV_CONVERSION
uniform mat4 yuvToRgb;
#endif

#if TRAIT_MODULATE
uniform vec4 modulation;
#endif

#if TRAIT_ADJUST_SATURATION
#include "saturation.glsl"
#endif

#if TRAIT_TRANSFORM_COLORSPACE
#include "colormanagement.glsl"
#endif

out vec4 fragColor;

void main(void)
{
    vec4 result;

#if TRAIT_MAP_TEXTURE
    result = texture(sampler, texcoord0);
#endif
#if TRAIT_MAP_MULTI_PLANE_TEXTURE
    result = vec4(texture(sampler, texcoord0).x, texture(sampler1, texcoord0).rg, 1.0);
#endif
#if TRAIT_MAP_EXTERNAL_TEXTURE
    // external textures require texture2D for sampling
    result = texture2D(sampler, texcoord0);
#endif
#if TRAIT_UNIFORM_COLOR
    result = geometryColor;
#endif

#if TRAIT_BORDER
    float inner = sdfRoundedBox(position0, box.xy, box.zw, cornerRadius);
    float outer = sdfRoundedBox(position0, box.xy, box.zw + vec2(thickness), cornerRadius + vec4(thickness));
    float f = sdfSubtract(outer, inner);
    float df = fwidth(f);
    result = geometryColor * (1.0 - clamp(0.5 + f / df, 0.0, 1.0));
#endif

#if TRAIT_YUV_CONVERSION
    result.rgb = (yuvToRgb * vec4(result.rgb, 1.0)).rgb;
#endif

#if TRAIT_ROUNDED_CORNERS
    float f = sdfRoundedBox(position0, box.xy, box.zw, cornerRadius);
    float df = fwidth(f);
    result *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
#endif

#if TRAIT_TRANSFORM_COLORSPACE
    result = encodingToNits(result, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    result.rgb = (colorimetryTransform * vec4(result.rgb, 1.0)).rgb;
#endif

#if TRAIT_ADJUST_SATURATION
    result = adjustSaturation(result);
#endif

#if TRAIT_MODULATE
    result *= modulation;
#endif

#if TRAIT_TRANSFORM_COLORSPACE
    result.rgb = doTonemapping(result.rgb);
    result = nitsToDestinationEncoding(result);
#endif

    fragColor = result;
}
