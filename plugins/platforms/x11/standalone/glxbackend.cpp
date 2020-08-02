/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "glxbackend.h"
#include "logging.h"
#include "glx_context_attribute_builder.h"
// kwin
#include "options.h"
#include "overlaywindow.h"
#include "composite.h"
#include "platform.h"
#include "scene.h"
#include "screens.h"
#include "xcbutils.h"
#include "texture.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <kwineffectquickview.h>
#include <kwinxrenderutils.h>
// Qt
#include <QDebug>
#include <QOpenGLContext>
#include <QX11Info>
#include <QtPlatformHeaders/QGLXNativeContext>
// system
#include <unistd.h>

#include <deque>
#include <algorithm>
#if HAVE_DL_LIBRARY
#include <dlfcn.h>
#endif

#ifndef XCB_GLX_BUFFER_SWAP_COMPLETE
#define XCB_GLX_BUFFER_SWAP_COMPLETE 1
typedef struct xcb_glx_buffer_swap_complete_event_t {
    uint8_t            response_type; /**<  */
    uint8_t            pad0; /**<  */
    uint16_t           sequence; /**<  */
    uint16_t           event_type; /**<  */
    uint8_t            pad1[2]; /**<  */
    xcb_glx_drawable_t drawable; /**<  */
    uint32_t           ust_hi; /**<  */
    uint32_t           ust_lo; /**<  */
    uint32_t           msc_hi; /**<  */
    uint32_t           msc_lo; /**<  */
    uint32_t           sbc; /**<  */
} xcb_glx_buffer_swap_complete_event_t;
#endif

#include <tuple>
#include <memory>

namespace KWin
{

SwapEventFilter::SwapEventFilter(xcb_drawable_t drawable, xcb_glx_drawable_t glxDrawable)
    : X11EventFilter(Xcb::Extensions::self()->glxEventBase() + XCB_GLX_BUFFER_SWAP_COMPLETE),
      m_drawable(drawable),
      m_glxDrawable(glxDrawable)
{
}

bool SwapEventFilter::event(xcb_generic_event_t *event)
{
    xcb_glx_buffer_swap_complete_event_t *ev =
            reinterpret_cast<xcb_glx_buffer_swap_complete_event_t *>(event);

    // The drawable field is the X drawable when the event was synthesized
    // by a WireToEvent handler, and the GLX drawable when the event was
    // received over the wire
    if (ev->drawable == m_drawable || ev->drawable == m_glxDrawable) {
        Compositor::self()->bufferSwapComplete();
        return true;
    }

    return false;
}


// -----------------------------------------------------------------------



GlxBackend::GlxBackend(Display *display)
    : OpenGLBackend()
    , m_overlayWindow(kwinApp()->platform()->createOverlayWindow())
    , window(None)
    , fbconfig(nullptr)
    , glxWindow(None)
    , ctx(nullptr)
    , m_bufferAge(0)
    , haveSwapInterval(false)
    , m_x11Display(display)
{
     // Ensures calls to glXSwapBuffers will always block until the next
     // retrace when using the proprietary NVIDIA driver. This must be
     // set before libGL.so is loaded.
     setenv("__GL_MaxFramesAllowed", "1", true);

     // Force initialization of GLX integration in the Qt's xcb backend
     // to make it call XESetWireToEvent callbacks, which is required
     // by Mesa when using DRI2.
     QOpenGLContext::supportsThreadedOpenGL();
}

static bool gs_tripleBufferUndetected = true;
static bool gs_tripleBufferNeedsDetection = false;

GlxBackend::~GlxBackend()
{
    if (isFailed()) {
        m_overlayWindow->destroy();
    }
    // TODO: cleanup in error case
    // do cleanup after initBuffer()
    cleanupGL();
    doneCurrent();
    EffectQuickView::setShareContext(nullptr);

    gs_tripleBufferUndetected = true;
    gs_tripleBufferNeedsDetection = false;

    if (ctx)
        glXDestroyContext(display(), ctx);

    if (glxWindow)
        glXDestroyWindow(display(), glxWindow);

    if (window)
        XDestroyWindow(display(), window);

    qDeleteAll(m_fbconfigHash);
    m_fbconfigHash.clear();

    overlayWindow()->destroy();
    delete m_overlayWindow;
}

typedef void (*glXFuncPtr)();

static glXFuncPtr getProcAddress(const char* name)
{
    glXFuncPtr ret = nullptr;
#if HAVE_EPOXY_GLX
    ret = glXGetProcAddress((const GLubyte*) name);
#endif
#if HAVE_DL_LIBRARY
    if (ret == nullptr)
        ret = (glXFuncPtr) dlsym(RTLD_DEFAULT, name);
#endif
    return ret;
}
glXSwapIntervalMESA_func glXSwapIntervalMESA;

void GlxBackend::init()
{
    // Require at least GLX 1.3
    if (!checkVersion()) {
        setFailed(QStringLiteral("Requires at least GLX 1.3"));
        return;
    }

    initExtensions();

    // resolve glXSwapIntervalMESA if available
    if (hasExtension(QByteArrayLiteral("GLX_MESA_swap_control"))) {
        glXSwapIntervalMESA = (glXSwapIntervalMESA_func) getProcAddress("glXSwapIntervalMESA");
    } else {
        glXSwapIntervalMESA = nullptr;
    }

    initVisualDepthHashTable();

    if (!initBuffer()) {
        setFailed(QStringLiteral("Could not initialize the buffer"));
        return;
    }

    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    // Initialize OpenGL
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(GlxPlatformInterface);
    options->setGlPreferBufferSwap(options->glPreferBufferSwap()); // resolve autosetting
    if (options->glPreferBufferSwap() == Options::AutoSwapStrategy)
        options->setGlPreferBufferSwap('e'); // for unknown drivers - should not happen
    glPlatform->printResults();
    initGL(&getProcAddress);

    // Check whether certain features are supported
    m_haveMESACopySubBuffer = hasExtension(QByteArrayLiteral("GLX_MESA_copy_sub_buffer"));
    m_haveMESASwapControl   = hasExtension(QByteArrayLiteral("GLX_MESA_swap_control"));
    m_haveEXTSwapControl    = hasExtension(QByteArrayLiteral("GLX_EXT_swap_control"));
    m_haveSGISwapControl    = hasExtension(QByteArrayLiteral("GLX_SGI_swap_control"));
    // only enable Intel swap event if env variable is set, see BUG 342582
    m_haveINTELSwapEvent    = hasExtension(QByteArrayLiteral("GLX_INTEL_swap_event"))
                                && qgetenv("KWIN_USE_INTEL_SWAP_EVENT") == QByteArrayLiteral("1");

    if (m_haveINTELSwapEvent) {
        m_swapEventFilter = std::make_unique<SwapEventFilter>(window, glxWindow);
        glXSelectEvent(display(), glxWindow, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }

    haveSwapInterval = m_haveMESASwapControl || m_haveEXTSwapControl || m_haveSGISwapControl;

    setSupportsBufferAge(false);

    if (hasExtension(QByteArrayLiteral("GLX_EXT_buffer_age"))) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }

    setSyncsToVBlank(false);
    setBlocksForRetrace(false);
    haveWaitSync = false;
    gs_tripleBufferNeedsDetection = false;
    m_swapProfiler.init();
    const bool wantSync = options->glPreferBufferSwap() != Options::NoSwapEncourage;
    if (wantSync && glXIsDirect(display(), ctx)) {
        if (haveSwapInterval) { // glXSwapInterval is preferred being more reliable
            setSwapInterval(1);
            setSyncsToVBlank(true);
            const QByteArray tripleBuffer = qgetenv("KWIN_TRIPLE_BUFFER");
            if (!tripleBuffer.isEmpty()) {
                setBlocksForRetrace(qstrcmp(tripleBuffer, "0") == 0);
                gs_tripleBufferUndetected = false;
            }
            gs_tripleBufferNeedsDetection = gs_tripleBufferUndetected;
        } else if (hasExtension(QByteArrayLiteral("GLX_SGI_video_sync"))) {
            unsigned int sync;
            if (glXGetVideoSyncSGI(&sync) == 0 && glXWaitVideoSyncSGI(1, 0, &sync) == 0) {
                setSyncsToVBlank(true);
                setBlocksForRetrace(true);
                haveWaitSync = true;
            } else
                qCWarning(KWIN_X11STANDALONE) << "NO VSYNC! glXSwapInterval is not supported, glXWaitVideoSync is supported but broken";
        } else
            qCWarning(KWIN_X11STANDALONE) << "NO VSYNC! neither glSwapInterval nor glXWaitVideoSync are supported";
    } else {
        // disable v-sync (if possible)
        setSwapInterval(0);
    }
    if (glPlatform->isVirtualBox()) {
        // VirtualBox does not support glxQueryDrawable
        // this should actually be in kwinglutils_funcs, but QueryDrawable seems not to be provided by an extension
        // and the GLPlatform has not been initialized at the moment when initGLX() is called.
        glXQueryDrawable = nullptr;
    }

    setIsDirectRendering(bool(glXIsDirect(display(), ctx)));

    qCDebug(KWIN_X11STANDALONE) << "Direct rendering:" << isDirectRendering();
}

bool GlxBackend::checkVersion()
{
    int major, minor;
    glXQueryVersion(display(), &major, &minor);
    return kVersionNumber(major, minor) >= kVersionNumber(1, 3);
}

void GlxBackend::initExtensions()
{
    const QByteArray string = (const char *) glXQueryExtensionsString(display(), QX11Info::appScreen());
    setExtensions(string.split(' '));
}

bool GlxBackend::initRenderingContext()
{
    const bool direct = true;

    // Use glXCreateContextAttribsARB() when it's available
    if (hasExtension(QByteArrayLiteral("GLX_ARB_create_context"))) {
        const bool have_robustness = hasExtension(QByteArrayLiteral("GLX_ARB_create_context_robustness"));
        const bool haveVideoMemoryPurge = hasExtension(QByteArrayLiteral("GLX_NV_robustness_video_memory_purge"));

        std::vector<GlxContextAttributeBuilder> candidates;
        if (options->glCoreProfile()) {
            if (have_robustness) {
                if (haveVideoMemoryPurge) {
                    GlxContextAttributeBuilder purgeMemoryCore;
                    purgeMemoryCore.setVersion(3, 1);
                    purgeMemoryCore.setRobust(true);
                    purgeMemoryCore.setResetOnVideoMemoryPurge(true);
                    candidates.emplace_back(std::move(purgeMemoryCore));
                }
                GlxContextAttributeBuilder robustCore;
                robustCore.setVersion(3, 1);
                robustCore.setRobust(true);
                candidates.emplace_back(std::move(robustCore));
            }
            GlxContextAttributeBuilder core;
            core.setVersion(3, 1);
            candidates.emplace_back(std::move(core));
        } else {
            if (have_robustness) {
                if (haveVideoMemoryPurge) {
                    GlxContextAttributeBuilder purgeMemoryLegacy;
                    purgeMemoryLegacy.setRobust(true);
                    purgeMemoryLegacy.setResetOnVideoMemoryPurge(true);
                    candidates.emplace_back(std::move(purgeMemoryLegacy));
                }
                GlxContextAttributeBuilder robustLegacy;
                robustLegacy.setRobust(true);
                candidates.emplace_back(std::move(robustLegacy));
            }
            GlxContextAttributeBuilder legacy;
            legacy.setVersion(2, 1);
            candidates.emplace_back(std::move(legacy));
        }
        for (auto it = candidates.begin(); it != candidates.end(); it++) {
            const auto attribs = it->build();
            ctx = glXCreateContextAttribsARB(display(), fbconfig, nullptr, true, attribs.data());
            if (ctx) {
                qCDebug(KWIN_X11STANDALONE) << "Created GLX context with attributes:" << &(*it);
                break;
            }
        }
    }

    if (!ctx)
        ctx = glXCreateNewContext(display(), fbconfig, GLX_RGBA_TYPE, nullptr, direct);

    if (!ctx) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to create an OpenGL context.";
        return false;
    }

    if (!glXMakeCurrent(display(), glxWindow, ctx)) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to make the OpenGL context current.";
        glXDestroyContext(display(), ctx);
        ctx = nullptr;
        return false;
    }

    auto qtContext = new QOpenGLContext;
    QGLXNativeContext native(ctx, display());
    qtContext->setNativeHandle(QVariant::fromValue(native));
    qtContext->create();
    EffectQuickView::setShareContext(std::unique_ptr<QOpenGLContext>(qtContext));

    return true;
}

bool GlxBackend::initBuffer()
{
    if (!initFbConfig())
        return false;

    if (overlayWindow()->create()) {
        xcb_connection_t * const c = connection();

        // Try to create double-buffered window in the overlay
        xcb_visualid_t visual;
        glXGetFBConfigAttrib(display(), fbconfig, GLX_VISUAL_ID, (int *) &visual);

        if (!visual) {
           qCCritical(KWIN_X11STANDALONE) << "The GLXFBConfig does not have an associated X visual";
           return false;
        }

        xcb_colormap_t colormap = xcb_generate_id(c);
        xcb_create_colormap(c, false, colormap, rootWindow(), visual);

        const QSize size = screens()->size();

        window = xcb_generate_id(c);
        xcb_create_window(c, visualDepth(visual), window, overlayWindow()->window(),
                          0, 0, size.width(), size.height(), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          visual, XCB_CW_COLORMAP, &colormap);

        glxWindow = glXCreateWindow(display(), fbconfig, window, nullptr);
        overlayWindow()->setup(window);
    } else {
        qCCritical(KWIN_X11STANDALONE) << "Failed to create overlay window";
        return false;
    }

    return true;
}

bool GlxBackend::initFbConfig()
{
    const int attribs[] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_RED_SIZE,       1,
        GLX_GREEN_SIZE,     1,
        GLX_BLUE_SIZE,      1,
        GLX_ALPHA_SIZE,     0,
        GLX_DEPTH_SIZE,     0,
        GLX_STENCIL_SIZE,   0,
        GLX_CONFIG_CAVEAT,  GLX_NONE,
        GLX_DOUBLEBUFFER,   true,
        0
    };

    const int attribs_srgb[] = {
        GLX_RENDER_TYPE,                  GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,                GLX_WINDOW_BIT,
        GLX_RED_SIZE,                     1,
        GLX_GREEN_SIZE,                   1,
        GLX_BLUE_SIZE,                    1,
        GLX_ALPHA_SIZE,                   0,
        GLX_DEPTH_SIZE,                   0,
        GLX_STENCIL_SIZE,                 0,
        GLX_CONFIG_CAVEAT,                GLX_NONE,
        GLX_DOUBLEBUFFER,                 true,
        GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, true,
        0
    };

    bool llvmpipe = false;

    // Note that we cannot use GLPlatform::driver() here, because it has not been initialized at this point
    if (hasExtension(QByteArrayLiteral("GLX_MESA_query_renderer"))) {
        const QByteArray device = glXQueryRendererStringMESA(display(), DefaultScreen(display()), 0, GLX_RENDERER_DEVICE_ID_MESA);
        if (device.contains(QByteArrayLiteral("llvmpipe"))) {
            llvmpipe = true;
        }
    }

    // Try to find a double buffered sRGB capable configuration
    int count = 0;
    GLXFBConfig *configs = nullptr;

    // Don't request an sRGB configuration with LLVMpipe when the default depth is 16. See bug #408594.
    if (!llvmpipe || Xcb::defaultDepth() > 16) {
        configs = glXChooseFBConfig(display(), DefaultScreen(display()), attribs_srgb, &count);
    }

    if (count == 0) {
        // Try to find a double buffered non-sRGB capable configuration
        configs = glXChooseFBConfig(display(), DefaultScreen(display()), attribs, &count);
    }

    struct FBConfig {
        GLXFBConfig config;
        int depth;
        int stencil;
    };

    std::deque<FBConfig> candidates;

    for (int i = 0; i < count; i++) {
        int depth, stencil;
        glXGetFBConfigAttrib(display(), configs[i], GLX_DEPTH_SIZE,   &depth);
        glXGetFBConfigAttrib(display(), configs[i], GLX_STENCIL_SIZE, &stencil);

        candidates.emplace_back(FBConfig{configs[i], depth, stencil});
    }

    if (count > 0)
        XFree(configs);

    std::stable_sort(candidates.begin(), candidates.end(), [](const FBConfig &left, const FBConfig &right) {
        if (left.depth < right.depth)
            return true;

        if (left.stencil < right.stencil)
            return true;

        return false;
    });

    if (candidates.size() > 0) {
        fbconfig = candidates.front().config;

        int fbconfig_id, visual_id, red, green, blue, alpha, depth, stencil, srgb;
        glXGetFBConfigAttrib(display(), fbconfig, GLX_FBCONFIG_ID,  &fbconfig_id);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_VISUAL_ID,    &visual_id);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_RED_SIZE,     &red);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_GREEN_SIZE,   &green);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_BLUE_SIZE,    &blue);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_ALPHA_SIZE,   &alpha);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_DEPTH_SIZE,   &depth);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_STENCIL_SIZE, &stencil);
        glXGetFBConfigAttrib(display(), fbconfig, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, &srgb);

        qCDebug(KWIN_X11STANDALONE, "Choosing GLXFBConfig %#x X visual %#x depth %d RGBA %d:%d:%d:%d ZS %d:%d sRGB: %d",
                fbconfig_id, visual_id, visualDepth(visual_id), red, green, blue, alpha, depth, stencil, srgb);
    }

    if (fbconfig == nullptr) {
        qCCritical(KWIN_X11STANDALONE) << "Failed to find a usable framebuffer configuration";
        return false;
    }

    return true;
}

void GlxBackend::initVisualDepthHashTable()
{
    const xcb_setup_t *setup = xcb_get_setup(connection());

    for (auto screen = xcb_setup_roots_iterator(setup); screen.rem; xcb_screen_next(&screen)) {
        for (auto depth = xcb_screen_allowed_depths_iterator(screen.data); depth.rem; xcb_depth_next(&depth)) {
            const int len = xcb_depth_visuals_length(depth.data);
            const xcb_visualtype_t *visuals = xcb_depth_visuals(depth.data);

            for (int i = 0; i < len; i++)
                m_visualDepthHash.insert(visuals[i].visual_id, depth.data->depth);
        }
    }
}

int GlxBackend::visualDepth(xcb_visualid_t visual) const
{
    return m_visualDepthHash.value(visual);
}

static inline int bitCount(uint32_t mask)
{
#if defined(__GNUC__)
    return __builtin_popcount(mask);
#else
    int count = 0;

    while (mask) {
        count += (mask & 1);
        mask >>= 1;
    }

    return count;
#endif
}

FBConfigInfo *GlxBackend::infoForVisual(xcb_visualid_t visual)
{
    auto it = m_fbconfigHash.constFind(visual);
    if (it != m_fbconfigHash.constEnd()) {
        return it.value();
    }

    FBConfigInfo *info = new FBConfigInfo;
    m_fbconfigHash.insert(visual, info);
    info->fbconfig            = nullptr;
    info->bind_texture_format = 0;
    info->texture_targets     = 0;
    info->y_inverted          = 0;
    info->mipmap              = 0;

    const xcb_render_pictformat_t format = XRenderUtils::findPictFormat(visual);
    const xcb_render_directformat_t *direct = XRenderUtils::findPictFormatInfo(format);

    if (!direct) {
        qCCritical(KWIN_X11STANDALONE).nospace() << "Could not find a picture format for visual 0x" << Qt::hex << visual;
        return info;
    }

    const int red_bits   = bitCount(direct->red_mask);
    const int green_bits = bitCount(direct->green_mask);
    const int blue_bits  = bitCount(direct->blue_mask);
    const int alpha_bits = bitCount(direct->alpha_mask);

    const int depth = visualDepth(visual);

    const auto rgb_sizes = std::tie(red_bits, green_bits, blue_bits);

    const int attribs[] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT | GLX_PIXMAP_BIT,
        GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
        GLX_X_RENDERABLE,   True,
        GLX_CONFIG_CAVEAT,  int(GLX_DONT_CARE), // The ARGB32 visual is marked non-conformant in Catalyst
        GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT,  int(GLX_DONT_CARE), // The ARGB32 visual is marked sRGB capable in mesa/i965
        GLX_BUFFER_SIZE,    red_bits + green_bits + blue_bits + alpha_bits,
        GLX_RED_SIZE,       red_bits,
        GLX_GREEN_SIZE,     green_bits,
        GLX_BLUE_SIZE,      blue_bits,
        GLX_ALPHA_SIZE,     alpha_bits,
        GLX_STENCIL_SIZE,   0,
        GLX_DEPTH_SIZE,     0,
        0
    };

    int count = 0;
    GLXFBConfig *configs = glXChooseFBConfig(display(), DefaultScreen(display()), attribs, &count);

    if (count < 1) {
        qCCritical(KWIN_X11STANDALONE).nospace() << "Could not find a framebuffer configuration for visual 0x" << Qt::hex << visual;
        return info;
    }

    struct FBConfig {
        GLXFBConfig config;
        int depth;
        int stencil;
        int format;
    };

    std::deque<FBConfig> candidates;

    for (int i = 0; i < count; i++) {
        int red, green, blue;
        glXGetFBConfigAttrib(display(), configs[i], GLX_RED_SIZE,   &red);
        glXGetFBConfigAttrib(display(), configs[i], GLX_GREEN_SIZE, &green);
        glXGetFBConfigAttrib(display(), configs[i], GLX_BLUE_SIZE,  &blue);

        if (std::tie(red, green, blue) != rgb_sizes)
            continue;

        xcb_visualid_t visual;
        glXGetFBConfigAttrib(display(), configs[i], GLX_VISUAL_ID, (int *) &visual);

        if (visualDepth(visual) != depth)
            continue;

        int bind_rgb, bind_rgba;
        glXGetFBConfigAttrib(display(), configs[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &bind_rgba);
        glXGetFBConfigAttrib(display(), configs[i], GLX_BIND_TO_TEXTURE_RGB_EXT,  &bind_rgb);

        if (!bind_rgb && !bind_rgba)
            continue;

        int depth, stencil;
        glXGetFBConfigAttrib(display(), configs[i], GLX_DEPTH_SIZE,   &depth);
        glXGetFBConfigAttrib(display(), configs[i], GLX_STENCIL_SIZE, &stencil);

        int texture_format;
        if (alpha_bits)
            texture_format = bind_rgba ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT;
        else
            texture_format = bind_rgb ? GLX_TEXTURE_FORMAT_RGB_EXT : GLX_TEXTURE_FORMAT_RGBA_EXT;

        candidates.emplace_back(FBConfig{configs[i], depth, stencil, texture_format});
    }

    if (count > 0)
        XFree(configs);

    std::stable_sort(candidates.begin(), candidates.end(), [](const FBConfig &left, const FBConfig &right) {
        if (left.depth < right.depth)
            return true;

        if (left.stencil < right.stencil)
            return true;

        return false;
    });

    if (candidates.size() > 0) {
        const FBConfig &candidate = candidates.front();

        int y_inverted, texture_targets;
        glXGetFBConfigAttrib(display(), candidate.config, GLX_BIND_TO_TEXTURE_TARGETS_EXT, &texture_targets);
        glXGetFBConfigAttrib(display(), candidate.config, GLX_Y_INVERTED_EXT, &y_inverted);

        info->fbconfig            = candidate.config;
        info->bind_texture_format = candidate.format;
        info->texture_targets     = texture_targets;
        info->y_inverted          = y_inverted;
        info->mipmap              = 0;
    }

    if (info->fbconfig) {
        int fbc_id = 0;
        int visual_id = 0;

        glXGetFBConfigAttrib(display(), info->fbconfig, GLX_FBCONFIG_ID, &fbc_id);
        glXGetFBConfigAttrib(display(), info->fbconfig, GLX_VISUAL_ID,   &visual_id);

        qCDebug(KWIN_X11STANDALONE).nospace() << "Using FBConfig 0x" << Qt::hex << fbc_id << " for visual 0x" << Qt::hex << visual_id;
    }

    return info;
}

void GlxBackend::setSwapInterval(int interval)
{
    if (m_haveEXTSwapControl)
        glXSwapIntervalEXT(display(), glxWindow, interval);
    else if (m_haveMESASwapControl)
        glXSwapIntervalMESA(interval);
    else if (m_haveSGISwapControl)
        glXSwapIntervalSGI(interval);
}

void GlxBackend::waitSync()
{
    // NOTE that vsync has no effect with indirect rendering
    if (haveWaitSync) {
        uint sync;
#if 0
        // TODO: why precisely is this important?
        // the sync counter /can/ perform multiple steps during glXGetVideoSync & glXWaitVideoSync
        // but this only leads to waiting for two frames??!?
        glXGetVideoSync(&sync);
        glXWaitVideoSync(2, (sync + 1) % 2, &sync);
#else
        glXWaitVideoSyncSGI(1, 0, &sync);
#endif
    }
}

void GlxBackend::present()
{
    if (lastDamage().isEmpty())
        return;

    const QSize &screenSize = screens()->size();
    const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());
    const bool fullRepaint = supportsBufferAge() || (lastDamage() == displayRegion);

    if (fullRepaint) {
        if (m_haveINTELSwapEvent)
            Compositor::self()->aboutToSwapBuffers();

        if (haveSwapInterval) {
            if (gs_tripleBufferNeedsDetection) {
                glXWaitGL();
                m_swapProfiler.begin();
            }
            glXSwapBuffers(display(), glxWindow);
            if (gs_tripleBufferNeedsDetection) {
                glXWaitGL();
                if (char result = m_swapProfiler.end()) {
                    gs_tripleBufferUndetected = gs_tripleBufferNeedsDetection = false;
                    setBlocksForRetrace(result == 'd');
                }
            }
        } else {
            waitSync();
            glXSwapBuffers(display(), glxWindow);
        }
        if (supportsBufferAge()) {
            glXQueryDrawable(display(), glxWindow, GLX_BACK_BUFFER_AGE_EXT, (GLuint *) &m_bufferAge);
        }
    } else if (m_haveMESACopySubBuffer) {
        for (const QRect &r : lastDamage()) {
            // convert to OpenGL coordinates
            int y = screenSize.height() - r.y() - r.height();
            glXCopySubBufferMESA(display(), glxWindow, r.x(), y, r.width(), r.height());
        }
    } else { // Copy Pixels (horribly slow on Mesa)
        glDrawBuffer(GL_FRONT);
        copyPixels(lastDamage());
        glDrawBuffer(GL_BACK);
    }

    setLastDamage(QRegion());
    if (!supportsBufferAge()) {
        glXWaitGL();
        XFlush(display());
    }
}

void GlxBackend::screenGeometryChanged(const QSize &size)
{
    doneCurrent();

    XMoveResizeWindow(display(), window, 0, 0, size.width(), size.height());
    overlayWindow()->setup(window);
    Xcb::sync();

    makeCurrent();
    glViewport(0, 0, size.width(), size.height());

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

SceneOpenGLTexturePrivate *GlxBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new GlxTexture(texture, this);
}

QRegion GlxBackend::prepareRenderingFrame()
{
    QRegion repaint;

    if (gs_tripleBufferNeedsDetection) {
        // the composite timer floors the repaint frequency. This can pollute our triple buffering
        // detection because the glXSwapBuffers call for the new frame has to wait until the pending
        // one scanned out.
        // So we compensate for that by waiting an extra milisecond to give the driver the chance to
        // fllush the buffer queue
        usleep(1000);
    }

    present();

    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);

    startRenderTimer();
    glXWaitX();

    return repaint;
}

void GlxBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    if (damagedRegion.isEmpty()) {
        setLastDamage(QRegion());

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.isEmpty())
            glFlush();

        m_bufferAge = 1;
        return;
    }

    setLastDamage(renderedRegion);

    if (!blocksForRetrace()) {
        // This also sets lastDamage to empty which prevents the frame from
        // being posted again when prepareRenderingFrame() is called.
        present();
    } else {
        // Make sure that the GPU begins processing the command stream
        // now and not the next time prepareRenderingFrame() is called.
        glFlush();
    }

    if (overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long

    // Save the damaged region to history
    if (supportsBufferAge())
        addToDamageHistory(damagedRegion);
}

bool GlxBackend::makeCurrent()
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool current = glXMakeCurrent(display(), glxWindow, ctx);
    return current;
}

void GlxBackend::doneCurrent()
{
    glXMakeCurrent(display(), None, nullptr);
}

OverlayWindow* GlxBackend::overlayWindow() const
{
    return m_overlayWindow;
}

bool GlxBackend::usesOverlayWindow() const
{
    return true;
}

/********************************************************
 * GlxTexture
 *******************************************************/
GlxTexture::GlxTexture(SceneOpenGLTexture *texture, GlxBackend *backend)
    : SceneOpenGLTexturePrivate()
    , q(texture)
    , m_backend(backend)
    , m_glxpixmap(None)
{
}

GlxTexture::~GlxTexture()
{
    if (m_glxpixmap != None) {
        if (!options->isGlStrictBinding()) {
            glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        }
        glXDestroyPixmap(display(), m_glxpixmap);
        m_glxpixmap = None;
    }
}

void GlxTexture::onDamage()
{
    if (options->isGlStrictBinding() && m_glxpixmap) {
        glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, nullptr);
    }
    GLTexturePrivate::onDamage();
}

bool GlxTexture::loadTexture(xcb_pixmap_t pixmap, const QSize &size, xcb_visualid_t visual)
{
    if (pixmap == XCB_NONE || size.isEmpty() || visual == XCB_NONE)
        return false;

    const FBConfigInfo *info = m_backend->infoForVisual(visual);
    if (!info || info->fbconfig == nullptr)
        return false;

    if (info->texture_targets & GLX_TEXTURE_2D_BIT_EXT) {
        m_target = GL_TEXTURE_2D;
        m_scale.setWidth(1.0f / m_size.width());
        m_scale.setHeight(1.0f / m_size.height());
    } else {
        Q_ASSERT(info->texture_targets & GLX_TEXTURE_RECTANGLE_BIT_EXT);

        m_target = GL_TEXTURE_RECTANGLE;
        m_scale.setWidth(1.0f);
        m_scale.setHeight(1.0f);
    }

    const int attrs[] = {
        GLX_TEXTURE_FORMAT_EXT, info->bind_texture_format,
        GLX_MIPMAP_TEXTURE_EXT, false,
        GLX_TEXTURE_TARGET_EXT, m_target == GL_TEXTURE_2D ? GLX_TEXTURE_2D_EXT : GLX_TEXTURE_RECTANGLE_EXT,
        0
    };

    m_glxpixmap     = glXCreatePixmap(display(), info->fbconfig, pixmap, attrs);
    m_size          = size;
    m_yInverted     = info->y_inverted ? true : false;
    m_canUseMipmaps = false;

    glGenTextures(1, &m_texture);

    q->setDirty();
    q->setFilter(GL_NEAREST);

    glBindTexture(m_target, m_texture);
    glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, nullptr);

    updateMatrix();
    return true;
}

bool GlxTexture::loadTexture(WindowPixmap *pixmap)
{
    Toplevel *t = pixmap->toplevel();
    return loadTexture(pixmap->pixmap(), t->bufferGeometry().size(), t->visual());
}

OpenGLBackend *GlxTexture::backend()
{
    return m_backend;
}

} // namespace
