The kernel provides an API to do color operations on drm planes, the `COLOR_PIPELINE`. This pipeline is a linked list of drm color operation objects, which represent usable hardware blocks of the GPU.

The currently available operations are
- 1D LUT
- 1D curve (named EOTF or inverse EOTF)
- 3x4 matrix
- multiplier
- 3D LUT

The color pipeline with AMD's DCN 3.1.4 for example is

- A: 1D curve
- B: multiplier
- C: 3x4 Matrix
- D: 1D curve
- E: 1D LUT
- F: 3D LUT
- G: 1D curve
- H: 1D LUT

Color operations can usually be bypassed, though some hardware has blocks that can't be bypassed individually (only the whole pipeline can be bypassed at once).

# Programming the color pipeline
We start from KWin's own `ColorPipeline`, a list of very similar operations. See [color-management.md](../../../doc/color-management.md) for more details on that.

The difficult part of programming our color pipeline into the drm color pipeline is that the drm color pipeline rarely exactly matches our color operations, and that some hardware blocks are more or less well suited for different color operation types than others.

The two main ideas we use in this algorithm are that
- at any point of the pipeline, we can add a new operation and its inverse, without changing the overall result. With this we can change the coordinate space between two hardware operations, like
    - `1D LUT operations + multiplier 1/range -> [0; 1] -> multiplier range + matrix` to avoid clipping the LUT's output
    - `matrix + inverse transfer function -> [0; 1] -> transfer function + 3D LUT` to change 3D LUT's input color space to non-linear RGB for improved resolution
- some operations can be split into multiple steps, like
    - matrix -> rgb multiplier + matrix
    - transfer function -> transfer function + scaling + offset
    - inverse transfer function -> offset + scaling + inverse transfer function

The algorithm is recursive, and roughly this:

1. the current color operation is either the previous operation's inverse output scaling if it exists, or the current operation from the `ColorPipeline`
2. for the current color operation, we check if the current hardware operation can represent it
    - if it can't, we start again, with the next hardware operation
    - if it can, we try to find a higher priority operation that can represent the same color operation and use that instead
    - if there's no higher priority operation, we continue with step 3
3. if necessary for the current color operation, we create an output scaling operation and its inverse
4. we recurse to step 1 with the next color operation, the inverse output scaling and the same hardware operation
5. we apply the color operations:
    - if the previous color operation was assigned to a different hardware operation than this one, we add our own input scaling
    - we add the actual operation
    - if the next color operation was assigned to a different hardware operation than this one, we add
        - our output scaling operation or
        - the next color operation's inverse input scaling
6. we return the data the previous recursion step needs

# Input and output scaling
A critical part of the algorithm is the scaling between hardware blocks. This is required due to hardware limitations - some blocks use normalized values in the kernel API, and are thus limited to a value range of [0; 1], while others are so limited in resolution that they need to be applied in non-linear space to not cause banding.

In order to deal with those limitations, the algorithm inserts additional color operations in between hardware blocks. For example, if we want to apply a transfer function with input range [0; 1] and output range [0; 200] in a 1D LUT, that output range cannot be represented with normalized values. To still be able to use the hardware block, we add an output scaling operation (2), and to not modify the end result, also its inverse (3).

The lines in between represent separations between hardware operations.

0. input with range [0; 1]

---

1. (original) transfer function [0; 200]
2. multiplier 1/200 [0; 1]

---

3. multiplier 200 [0; 200]

Steps 1 and 2 combined have input and output ranges of [0; 1], so the LUT can be programmed - as long as there is another hardware block after it that can apply step 3, the inverse output scaling.

The exact same principle applies for hardware operations with limited resolution, 3D LUTs. Here we may insert an input scaling operation (1) and its inverse (2) before a matrix, and an output scaling operation (4) and its inverse (5) after the matrix:

0. input with range [0; 1] in linear RGB
1. custom inverse transfer function [0; 1]  -> nonlinear RGB

---

2. custom transfer function [0; 1]          -> linear RGB
3. (original) matrix                        -> linear RGB
4. custom inverse transfer function [0; 1]  -> nonlinear RGB

---

5. custom transfer function [0; 1]          -> linear RGB

Another optimization that can be applied using the same mechanism is to partially apply a color operation. Let's say we had a multiplier of 200, but the hardware block can only apply a multiplier of 100. We'd apply that multiplier of 100, and add an output scaling operation (2) and its inverse (3)

0. input with range [0; 1]

---

1. (original) multiplier 200
2. multiplier 1/2

---

3. multiplier 2

# Example
Note that for simplification, this example assumes that the 3D LUT has high enough resolution to deal with linear RGB.

A basic `ColorPipeline` could be

0. input with range [0; 1]
1. gamma 2.2 transfer function with min/max luminance [0; 200] and output range [0; 200]
2. matrix with output range [0; 200]
3. gamma 2.2 inverse transfer function with min/max luminance [0; 400] and output range [0; ~0.73]

Let's match that to the AMD color pipeline, (recursive) step by step:
We start by trying to assign color operation 1, a gamma 2.2 transfer function

1. none of drm color operations [A; D] can represent the gamma 2.2 transfer function, so we skip them.
2. E (1D LUT) can represent it
    - there's no higher priority hardware operation for the transfer function
    - the output range of the 1D LUT is [0; 1], so we calculate an output scaling factor of 1/200, and its inverse, 200
3. now we're looking at color operation 2. E can't represent a matrix, so we switch to the next hardware operation
4. now we're looking at the inverse output scaling factor 200 and hardware operation F (3D LUT)
    - we search for a higher priority hardware operation for the factor, but it's not successful
5. now we're looking at color operation 2 again, and still hardware operation F:
    - there's no higher priority hardware operation for the matrix
    - F has an output range of [0; 1], so we calculate an output scaling factor 1/200 to F, and its inverse, 200
6. now we're looking at the inverse output scaling 200. F can represent it
    - there's a higher priority hardware operation for the inverse output scaling factor 200, hardware operation H (1D LUT)
7. H can represent the inverse output scaling factor 200
    - there's no higher priority hardware operation for it
    - we create an output scaling factor of 1/200 and its inverse, 200
    - we step to color operation 3
8. H can represent the gamma 2.2 inverse transfer function
    - there's no higher priority hardware operation for it
    - we ignore the output scaling factor, as it's the same hardware operation

Now the recursion gets wrapped up:

8. stores for H the gamma 2.2 inverse transfer function
7. stores for H the inverse output scaling factor 200
6. does nothing
4. stores for F the inverse output scaling factor of 200
5. stores for F
    - the matrix
    - the output scaling factor of 1/200
3. does nothing
2. stores for E
    - the gamma 2.2 transfer function
    - the output scaling factor of 1/200
1. does nothing

In the end, the map of hardware op -> color ops contains:

- \[A; D\]: nothing
- E:
    - gamma 2.2 transfer function [0; 200]
    - output scaling factor 1/200 [0; 1]
- F:
    - inverse output scaling factor 200 [0; 200]
    - matrix [0; 200]
    - output scaling factor 1/200 [0; 1]
- G: nothing
- H:
    - inverse output scaling factor 200 [0; 200]
    - gamma 2.2 inverse transfer function [0; ~0.73]
    
![drm colorops diagram.svg](drm colorops diagram.svg)
