
Documentation on the drm/kms API is sparse and split up in a few places and files, mostly in the Linux kernel code.
This file is meant to provide an overview for drm/kms and gbm to provide a starting point for understanding how the APIs and the drm backend work.

# drm/kms

In the drm API there is a few objects that we care about:

- `connector`s. A drm connector represents either an actual physical connector out of your graphics card or, in the case of DisplayPort MultiStream, a virtual one
- `crtc`s. A drm crtc represents hardware that can drive a connector uniquely - be it physical or virtual. Crtcs can also drive multiple connectors by cloning, aka showing the same stuff on all of them. The number of crtcs on your GPU or display device is a hard limit of how many display you can drive with it
- `plane`s. A drm plane represents scanout hardware that can be used for hardware composition, so cropping, scaling and rotation. There is multiple types of planes:

    * `primary`: it can be asssumed that without the primary plane the output won't work. The kernel will always expose one per crtc
    * `cursor`: cursor planes are what they sound like, they allow to use special hardware built for cursors. They have special restrictions like size (DRM_CAP_CURSOR_WIDTH, DRM_CAP_CURSOR_HEIGHT), scaling (on AMD gpus its scaling must be the same as with the primary scale) and even position on some very funky hardware (Apple stuff). Cursor planes are always optional
    * `overlay`: overlay planes are what they sound like as well, they allow to use special hardware built for overlays (or underlays). The restrictions on this are of arbitrary complexity, you can never just assume they work. Overlay planes are also always optional
- `framebuffer`s, fb for short. These represent some sort of buffer that we'd like to show up on the screen and have to be created and destroyed by us
- `encoder`s. Can effectively be ignored, they were exposed in the API more or less by mistake and are just there for backwards compatibility

All drm objects have properties with a name and a value. Depending on the type this value can have a different meaning; some are a bitfield, some are just integer values and some are for arbitrary data blobs. Properties can be read-only (immutable) and are only informational, some are needed for functionality.

There's two APIs for drm, legacy and atomic modesetting (AMS).

Legacy only exposes connectors and crtcs, and only some of their properties. You first enable a connector and set a mode with `drmModeSetCrtc` and then push new frames with `drmModePageFlip`. `drmModePageFlip` has two flags we care about:

- `DRM_MODE_PAGE_FLIP_EVENT` tells the kernel to generate a page flip event for the crtc, which tells us when the new framebuffer has actually been set / when the old one is not needed anymore
- `DRM_MODE_PAGE_FLIP_ASYNC` tells the kernel that it should immediately apply the new framebuffer without waiting. This may cause tearing

For dynamic power management (dpms) you set the dpms property with `drmModeObjectSetProperty` and the kernel will handle the rest behind the scenes, or fail the request. Same story with `VRR_ENABLED`, `overscan` and similar.

With atomic modesetting all objects and properties are exposed. AMS works very differently from legacy: it has one generic function `drmModeAtomicCommit` that is used for pretty much everything. How this function works is that you first fill a `drmModeAtomicReq` with the properties you want to set, then you call `drmModeAtomicCommit` with some combination of flags. These flags decide on what the function actually does:

- `DRM_MODE_ATOMIC_TEST_ONLY` only tests whether or not the configuration would work but is guaranteed to not change anything
- `DRM_MODE_PAGE_FLIP_EVENT` tells the kernel that it should generate a page flip event for all crtcs that we change in the commit
- `DRM_MODE_ATOMIC_NONBLOCK` tells the kernel to make this function not blocking; it should not wait until things are actually applied before returning
- `DRM_MODE_ATOMIC_ALLOW_MODESET` tells the kernel that it is allowed to make our changes happen with a modeset, that is an event that can cause the display(s) to flicker or black out for a moment
- `DRM_MODE_PAGE_FLIP_ASYNC` is currently *not* supported. All requests with this flag set fail

Some upstream documentation can be found at https://www.kernel.org/doc/html/latest/gpu/drm-kms.html, https://01.org/linuxgraphics/gfx-docs/drm/drm-kms-properties.html and in the files at https://github.com/torvalds/linux/tree/master/drivers/gpu/drm.

For a lot of documentation on properties and capabilities of devices there's also https://drmdb.emersion.fr/

# gbm

The generic buffer manager API allows us to allocate buffers in graphics memory with a few properties. It's a relatively straight forward API:

- `gbm_bo` is a gbm buffer. It can be manually created and destroyed
- `gbm_surface` is a gbm surface, which allows us to create an egl surface that's using gbm buffers. With it we can render in egl and then create framebuffers from the things rendered in egl and present them on the display
- the `GBM_FORMAT_*` defines are just copies of the `DRM_FORMAT_*` defines in drm_fourcc.h and describe a buffer format. For example `DRM_FORMAT_XRGB8888` describes a buffer with 8 bits of red, 8 bits of green, 8 bits of blue and 8 bits of unused alpha (that's what the `X` stands for). Do not use the `GBM_BO_FORMAT_*` enum, it can cause problems! In general, ignore the buffer formats from the gbm header and instead use what drm_fourcc.h provides
- modifiers describe the actual memory layout that needs to be assumed for accessing the buffer. Older drivers like `radeon` don't support modifiers at all, on the other end of the spectrum the NVidia driver requires them. When we don't use functions that have us explicitly provide modifiers that's called an "implicit modifier" - that means the driver automatically picks a modifier for the use case. With implicit modifiers we have no guarantees about multi-gpu compatibility by default, instead the `GBM_BO_USE_LINEAR` usage flag has to be set when creating the buffer to enforce a linear format that all drivers can access without messing up the image

For gbm most of the upstream documentation is contained in https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gbm/main/gbm.c
