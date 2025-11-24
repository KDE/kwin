
This document describes KWin's color management approach and general architecture. Some knowledge about color science and color management in general is expected however, https://gitlab.freedesktop.org/pq/color-and-hdr is better suited for learning that.

# Color space descriptions
In KWin, a `ColorDescription` provides a color space with all the important properties that we need to describe surfaces and (most) outputs. Among other things, it contains the
- container colorimetry, which is a set of primaries and white point that can be used to convert between RGB and XYZ
- transfer function, which assigns each "electrical" color channel value to a luminance value in cd/m². They additionally use
    - "minimum" luminance, which is the luminance of electrical value 0
    - "maximum" luminance, which is the luminance of electrical value 1. Values above 1 can represent higher luminance than this!
- reference luminance, which defines the luminance of SDR / graphics white
- YUV coefficients, which describes how to go from YCbCr to RGB
- encoding range: limited or full range, in either YCbCr or RGB

There's also some additional metadata about
- the mastering display colorimetry, which describes which colors the mastering display used to author the image could present
    - when used to describe a real display, it instead describes the native primaries of that display
- minimum, maximum average and maximum peak luminance, which describe which luminance ranges the image actually uses
    - when used to describe a real display, it instead describes the luminance capability of that display
- the SDR colorimetry, which replaces primaries of sRGB content when it's rendered on the framebuffer with this `ColorDescription`
    - this is limited to the perceptual rendering intent, so applications with more strict requirements on color accuracy don't get messed with
    
# Color pipelines
A `ColorPipeline` describes a transformation between two color spaces. It's ultimately just a list of abstract mathematical operations, with some additional metadata to make applying it easier. Supported operations are
- transfer functions
- inverse transfer functions
- matrices
- multipliers
- tonemappers
- 1D LUTs
- 3D LUTs

The metadata of each operation consists of
- min value
- max value
- type of colorspace (like linear RGB, nonlinear RGB, ICtCp)

for input and output values each. In the `ColorPipeline`, output metadata of each operation matches the input metadata of the next.


The exact steps added to a `ColorPipeline` vary, depending on the use case. The basic `ColorDescription`->`ColorDescription` pipeline is created with `ColorPipeline::create`, which generates for example these operations:
1. transfer function of source color description
    - input range: [0; 1]
    - input colorspace type: nonlinear rgb (encoded values)
    - output range: [0.2, 80]
    - output colorspace type: linear rgb (luminance in cd/m²)
2. RGB->RGB matrix
    - contains
        - RGB->RGB black point compensation + reference luminance scaling for source transfer function
        - (scale-invariant) RGB->XYZ of source container colorimetry
        - (scale-invariant) XYZ->XYZ white point adaptation
        - (scale-invariant) XYZ->RGB of destination container colorimetry
        - RGB->RGB black point compensation + reference luminance scaling for destination transfer function
    - input range: [0.2, 80]
    - input colorspace type: linear rgb
    - output range: [0, 100]
    - output colorspace type: linear rgb
3. inverse transfer function of destination color description
    - input range: [0; 100]
    - input colorspace type: linear rgb
    - output range: [0; 1]
    - output colorspace type: nonlinear rgb

More complex operations like tonemapping can be added in between, and more operations can be added afterwards as well. To apply an ICC profile, you might for example undo the transfer function from step 3 by adding

4. transfer function of destination color description
    - input range: [0; 1]
    - input colorspace type: nonlinear rgb
    - output range: [0; 100]
    - output colorspace type: linear rgb
    
and replacing it with the inverse 'transfer function' / TRC LUTs + VCGT from the ICC profile:

5. multiplier `1.0 / reference`
    - input range: [0; 100]
    - input colorspace type: linear rgb
    - output range: [0; 1]
    - output colorspace type: linear rgb
6. inverse transfer function of ICC profile (1D LUT. Note that this requires normalized input and output values, hence the multiplier)
    - input range: [0; 1]
    - input colorspace type: linear rgb
    - output range: [0; 1]
    - output colorspace type: nonlinear rgb
7. VCGT of ICC profile (1D LUT)
    - input range: [0; 1]
    - input colorspace type: nonlinear rgb
    - output range: [0; 1]
    - output colorspace type: nonlinear rgb

When operations are added, if possible, `ColorPipeline` automatically combines them with the previous operations. For example, inverse transfer function + transfer function of the same type + luminance are automatically removed, and multipliers are multiplied in with matrices and transfer functions. The above pipeline would result in
1. transfer function of source color description
2. RGB->RGB matrix + multiplier
3. inverse transfer function of ICC profile (1D LUT)
4. VCGT of ICC profile (1D LUT)

# Blending
KWin creates a blending space in some gamma 2.2 format, the specifics of which depend on the output and its settings. It can for example be
- the native colorspace of an sRGB screen, BT709 primaries + gamma 2.2 transfer function with max luminance = reference luminance
- a modified verison of an ICC profile's colorspace, native primaries + gamma 2.2 transfer function with max luminance = reference luminance
- the native colorspace of a FreeSync HDR screen, native primaries + gamma 2.2 transfer function with max luminance = max display luminance
- a modified version of the BT2020PQ colorspace, BT2020 primaries + gamma 2.2 transfer function with max luminance = max display luminance

This provides benefits over linear blending, which is traditionally assumed to be the most "correct":
- gamma 2.2 blending is what applications were designed for, so visuals look good without changes
- gamma 2.2 encoding means that we don't need nearly as much color resolution to avoid banding as with linear
- keeping the value range limited to [0; 1] means we don't have to use blending shaders or floating point framebuffers

# Compositing
Each `Item` gets individually mapped to the blending color description of the output, then blending is done, and last the result is transformed to the output signaling colorspace before it's sent to the display.

Any of these steps can be applied with a shader, setting the appropriate image description for the host compositor in the nested Wayland backend, or with KMS in the drm backend.

# Night light
Night light is also applied with color management - when it's active, the drm backend
- changes the white point of the blending `ColorDescription` on each output to the warmer color temperature
- reduces reference and max luminance to avoid clipping colors
- applies a post-blending absolute colorimetric transform without white point adaptation to the original `ColorDescription` of the display

The adjusted `ColorDescription` means white point adaptation is done during compositing and keeps colors as correct as possible, while the last step effectively boils down to applying RGB factors on the image to actually change the white point.

# Screenshots, screencasting and color picking
All three operations re-render the scene in a separate framebuffer, usually sRGB. This ensures sRGB surfaces have exactly unmodified colors, and HDR images are tonemapped to match the limited dynamic range of screenshots and SDR videos.
