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
#include "glxconvenience.h"
#include "logging.h"
#include "glx_context_attribute_builder.h"
#include "omlsynccontrolvsyncmonitor.h"
#include "sgivideosyncvsyncmonitor.h"
#include "softwarevsyncmonitor.h"
#include "x11_platform.h"
// kwin
#include "options.h"
#include "overlaywindow.h"
#include "composite.h"
#include "platform.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include "surfaceitem_x11.h"
#include "utils/xcbutils.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <kwinoffscreenquickview.h>
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
    const xcb_glx_buffer_swap_complete_event_t *swapEvent =
            reinterpret_cast<xcb_glx_buffer_swap_complete_event_t *>(event);
    if (swapEvent->drawable != m_drawable && swapEvent->drawable != m_glxDrawable) {
        return false;
    }

    // The clock for the UST timestamp is left unspecified in the spec, however, usually,
    // it's CLOCK_MONOTONIC, so no special conversions are needed.
    const std::chrono::microseconds timestamp((uint64_t(swapEvent->ust_hi) << 32) | swapEvent->ust_lo);

    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(kwinApp()->platform()->renderLoop());
    renderLoopPrivate->notifyFrameCompleted(timestamp);

    return true;
}

GlxBackend::GlxBackend(Display *display, X11StandalonePlatform *backend)
    : OpenGLBackend()
    , m_overlayWindow(kwinApp()->platform()->createOverlayWindow())
    , window(None)
    , fbconfig(nullptr)
    , glxWindow(None)
    , ctx(nullptr)
    , m_bufferAge(0)
    , m_x11Display(display)
    , m_backend(backend)
{
     // Force initialization of GLX integration in the Qt's xcb backend
     // to make it call XESetWireToEvent callbacks, which is required
     // by Mesa when using DRI2.
     QOpenGLContext::supportsThreadedOpenGL();

     connect(screens(), &Screens::sizeChanged, this, &GlxBackend::screenGeometryChanged);
}

GlxBackend::~GlxBackend()
{
    delete m_vsyncMonitor;

    // No completion events will be received for in-flight frames, this may lock the
    // render loop. We need to ensure that the render loop is back to its initial state
    // if the render backend is about to be destroyed.
    RenderLoopPrivate::get(kwinApp()->platform()->renderLoop())->invalidate();

    if (isFailed()) {
        m_overlayWindow->destroy();
    }
    // TODO: cleanup in error case
    // do cleanup after initBuffer()
    cleanupGL();
    doneCurrent();

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

    m_renderTarget.reset(new GLRenderTarget(0, screens()->size()));

    bool supportsSwapEvent = false;

    if (hasExtension(QByteArrayLiteral("GLX_INTEL_swap_event"))) {
        if (qEnvironmentVariableIsSet("KWIN_USE_INTEL_SWAP_EVENT")) {
            supportsSwapEvent = qEnvironmentVariable("KWIN_USE_INTEL_SWAP_EVENT") != QLatin1String("0");
        } else {
            // Don't use swap events on Intel. The issue with Intel GPUs is that they are not as
            // powerful as discrete GPUs. Therefore, it's better to push frames as often as vblank
            // notifications are received. This, however, may increase latency. If the swap events
            // are enabled explicitly by setting the environment variable, honor that choice.
            supportsSwapEvent = !glPlatform->isIntel();
        }
    }

    // Check whether certain features are supported
    m_haveMESACopySubBuffer = hasExtension(QByteArrayLiteral("GLX_MESA_copy_sub_buffer"));
    m_haveMESASwapControl   = hasExtension(QByteArrayLiteral("GLX_MESA_swap_control"));
    m_haveEXTSwapControl    = hasExtension(QByteArrayLiteral("GLX_EXT_swap_control"));
    m_haveSGISwapControl    = hasExtension(QByteArrayLiteral("GLX_SGI_swap_control"));

    bool haveSwapInterval = m_haveMESASwapControl || m_haveEXTSwapControl || m_haveSGISwapControl;

    setSupportsBufferAge(false);

    if (hasExtension(QByteArrayLiteral("GLX_EXT_buffer_age"))) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }

    // If the buffer age extension is unsupported, glXSwapBuffers() is not guaranteed to
    // be called. Therefore, there is no point for creating the swap event filter.
    if (!supportsBufferAge()) {
        supportsSwapEvent = false;
    }

    static bool syncToVblankDisabled = qEnvironmentVariableIsSet("KWIN_X11_NO_SYNC_TO_VBLANK");
    if (!syncToVblankDisabled) {
        if (haveSwapInterval) {
            setSwapInterval(1);
        } else {
            qCWarning(KWIN_X11STANDALONE) << "glSwapInterval is unsupported";
        }
    } else {
        setSwapInterval(0); // disable vsync if possible
    }

    if (glPlatform->isVirtualBox()) {
        // VirtualBox does not support glxQueryDrawable
        // this should actually be in kwinglutils_funcs, but QueryDrawable seems not to be provided by an extension
        // and the GLPlatform has not been initialized at the moment when initGLX() is called.
        glXQueryDrawable = nullptr;
    }

    static bool forceSoftwareVsync = qEnvironmentVariableIntValue("KWIN_X11_FORCE_SOFTWARE_VSYNC");
    if (supportsSwapEvent && !forceSoftwareVsync) {
        // Nice, the GLX_INTEL_swap_event extension is available. We are going to receive
        // the presentation timestamp (UST) after glXSwapBuffers() via the X command stream.
        m_swapEventFilter = std::make_unique<SwapEventFilter>(window, glxWindow);
        glXSelectEvent(display(), glxWindow, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    } else {
        // If the GLX_INTEL_swap_event extension is unavailble, we are going to wait for
        // the next vblank event after swapping buffers. This is a bit racy solution, e.g.
        // the vblank may occur right in between querying video sync counter and the act
        // of swapping buffers, but on the other hand, there is no any better alternative
        // option. NVIDIA doesn't provide any extension such as GLX_INTEL_swap_event.
        if (!forceSoftwareVsync) {
            if (!m_vsyncMonitor) {
                m_vsyncMonitor = SGIVideoSyncVsyncMonitor::create(this);
            }
            if (!m_vsyncMonitor) {
                m_vsyncMonitor = OMLSyncControlVsyncMonitor::create(this);
            }
        }
        if (!m_vsyncMonitor) {
            SoftwareVsyncMonitor *monitor = SoftwareVsyncMonitor::create(this);
            RenderLoop *renderLoop = m_backend->renderLoop();
            monitor->setRefreshRate(renderLoop->refreshRate());
            connect(renderLoop, &RenderLoop::refreshRateChanged, this, [this, monitor]() {
                monitor->setRefreshRate(m_backend->renderLoop()->refreshRate());
            });
            m_vsyncMonitor = monitor;
        }

        connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &GlxBackend::vblank);
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

    QOpenGLContext *qtGlobalShareContext = QOpenGLContext::globalShareContext();
    GLXContext globalShareContext = nullptr;
    if (qtGlobalShareContext) {
        qDebug(KWIN_X11STANDALONE) << "Global share context format:" << qtGlobalShareContext->format();
        const QVariant nativeHandle = qtGlobalShareContext->nativeHandle();
        if (!nativeHandle.canConvert<QGLXNativeContext>()) {
            qCDebug(KWIN_X11STANDALONE) << "Invalid QOpenGLContext::globalShareContext()";
            return false;
        } else {
            QGLXNativeContext handle = qvariant_cast<QGLXNativeContext>(nativeHandle);
            globalShareContext = handle.context();
        }
    }
    if (!globalShareContext) {
        qCWarning(KWIN_X11STANDALONE) << "QOpenGLContext::globalShareContext() is required";
        return false;
    }

    // Use glXCreateContextAttribsARB() when it's available
    if (hasExtension(QByteArrayLiteral("GLX_ARB_create_context"))) {
        const bool have_robustness = hasExtension(QByteArrayLiteral("GLX_ARB_create_context_robustness"));
        const bool haveVideoMemoryPurge = hasExtension(QByteArrayLiteral("GLX_NV_robustness_video_memory_purge"));

        std::vector<GlxContextAttributeBuilder> candidates;
        // core
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
        // legacy
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
        for (auto it = candidates.begin(); it != candidates.end(); it++) {
            const auto attribs = it->build();
            ctx = glXCreateContextAttribsARB(display(), fbconfig, globalShareContext, true, attribs.data());
            if (ctx) {
                qCDebug(KWIN_X11STANDALONE) << "Created GLX context with attributes:" << &(*it);
                break;
            }
        }
    }

    if (!ctx)
        ctx = glXCreateNewContext(display(), fbconfig, GLX_RGBA_TYPE, globalShareContext, direct);

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

    // Only request sRGB configurations with default depth 24 as it can cause problems with other default depths. See bugs #408594 and #423014.
    if (Xcb::defaultDepth() == 24) {
        fbconfig = chooseGlxFbConfig(display(), attribs_srgb);
    }
    if (!fbconfig) {
        fbconfig = chooseGlxFbConfig(display(), attribs);
    }

    if (fbconfig == nullptr) {
        qCCritical(KWIN_X11STANDALONE) << "Failed to find a usable framebuffer configuration";
        return false;
    }

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
        GLX_X_RENDERABLE,   true,
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

void GlxBackend::present(const QRegion &damage)
{
    const QSize &screenSize = screens()->size();
    const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());
    const bool fullRepaint = supportsBufferAge() || (damage == displayRegion);

    if (fullRepaint) {
        glXSwapBuffers(display(), glxWindow);
        if (supportsBufferAge()) {
            glXQueryDrawable(display(), glxWindow, GLX_BACK_BUFFER_AGE_EXT, (GLuint *) &m_bufferAge);
        }
    } else if (m_haveMESACopySubBuffer) {
        for (const QRect &r : damage) {
            // convert to OpenGL coordinates
            int y = screenSize.height() - r.y() - r.height();
            glXCopySubBufferMESA(display(), glxWindow, r.x(), y, r.width(), r.height());
        }
    } else { // Copy Pixels (horribly slow on Mesa)
        glDrawBuffer(GL_FRONT);
        copyPixels(damage);
        glDrawBuffer(GL_BACK);
    }

    if (!supportsBufferAge()) {
        glXWaitGL();
        XFlush(display());
    }
}

void GlxBackend::screenGeometryChanged()
{
    const QSize size = screens()->size();
    doneCurrent();

    XMoveResizeWindow(display(), window, 0, 0, size.width(), size.height());
    overlayWindow()->setup(window);
    Xcb::sync();

    // The back buffer contents are now undefined
    m_bufferAge = 0;
    m_renderTarget.reset(new GLRenderTarget(0, size));
}

SurfaceTexture *GlxBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return new GlxSurfaceTextureX11(this, pixmap);
}

QRegion GlxBackend::beginFrame(AbstractOutput *output)
{
    Q_UNUSED(output)

    QRegion repaint;
    makeCurrent();

    GLRenderTarget::pushRenderTarget(m_renderTarget.data());
    if (supportsBufferAge()) {
        repaint = m_damageJournal.accumulate(m_bufferAge, screens()->geometry());
    }

    glXWaitX();

    return repaint;
}

void GlxBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(output)

    // If the GLX_INTEL_swap_event extension is not used for getting presentation feedback,
    // assume that the frame will be presented at the next vblank event, this is racy.
    if (m_vsyncMonitor) {
        m_vsyncMonitor->arm();
    }

    const QRegion displayRegion(screens()->geometry());

    QRegion effectiveRenderedRegion = renderedRegion;
    if (!supportsBufferAge() && options->glPreferBufferSwap() == Options::CopyFrontBuffer && renderedRegion != displayRegion) {
        glReadBuffer(GL_FRONT);
        copyPixels(displayRegion - renderedRegion);
        glReadBuffer(GL_BACK);
        effectiveRenderedRegion = displayRegion;
    }

    GLRenderTarget::popRenderTarget();

    present(effectiveRenderedRegion);

    if (overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long

    // Save the damaged region to history
    if (supportsBufferAge()) {
        m_damageJournal.add(damagedRegion);
    }
}

void GlxBackend::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_backend->renderLoop());
    renderLoopPrivate->notifyFrameCompleted(timestamp);
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

GlxSurfaceTextureX11::GlxSurfaceTextureX11(GlxBackend *backend, SurfacePixmapX11 *texture)
    : OpenGLSurfaceTextureX11(backend, texture)
{
}

bool GlxSurfaceTextureX11::create()
{
    auto texture = new GlxPixmapTexture(static_cast<GlxBackend *>(m_backend));
    if (texture->create(m_pixmap)) {
        m_texture.reset(texture);
    }
    return !m_texture.isNull();
}

void GlxSurfaceTextureX11::update(const QRegion &region)
{
    Q_UNUSED(region)
    // mipmaps need to be updated
    m_texture->setDirty();
}

GlxPixmapTexture::GlxPixmapTexture(GlxBackend *backend)
    : GLTexture(*new GlxPixmapTexturePrivate(this, backend))
{
}

bool GlxPixmapTexture::create(SurfacePixmapX11 *texture)
{
    Q_D(GlxPixmapTexture);
    return d->create(texture);
}

GlxPixmapTexturePrivate::GlxPixmapTexturePrivate(GlxPixmapTexture *texture, GlxBackend *backend)
    : m_backend(backend)
    , q(texture)
    , m_glxPixmap(None)
{
}

GlxPixmapTexturePrivate::~GlxPixmapTexturePrivate()
{
    if (m_glxPixmap != None) {
        if (!options->isGlStrictBinding()) {
            glXReleaseTexImageEXT(m_backend->display(), m_glxPixmap, GLX_FRONT_LEFT_EXT);
        }
        glXDestroyPixmap(m_backend->display(), m_glxPixmap);
        m_glxPixmap = None;
    }
}

void GlxPixmapTexturePrivate::onDamage()
{
    if (options->isGlStrictBinding() && m_glxPixmap) {
        glXReleaseTexImageEXT(m_backend->display(), m_glxPixmap, GLX_FRONT_LEFT_EXT);
        glXBindTexImageEXT(m_backend->display(), m_glxPixmap, GLX_FRONT_LEFT_EXT, nullptr);
    }
    GLTexturePrivate::onDamage();
}

bool GlxPixmapTexturePrivate::create(SurfacePixmapX11 *texture)
{
    if (texture->pixmap() == XCB_NONE || texture->size().isEmpty() || texture->visual() == XCB_NONE)
        return false;

    const FBConfigInfo *info = m_backend->infoForVisual(texture->visual());
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

    m_glxPixmap     = glXCreatePixmap(m_backend->display(), info->fbconfig, texture->pixmap(), attrs);
    m_size          = texture->size();
    m_yInverted     = info->y_inverted ? true : false;
    m_canUseMipmaps = false;

    glGenTextures(1, &m_texture);

    q->setDirty();
    q->setFilter(GL_NEAREST);

    glBindTexture(m_target, m_texture);
    glXBindTexImageEXT(m_backend->display(), m_glxPixmap, GLX_FRONT_LEFT_EXT, nullptr);

    updateMatrix();
    return true;
}

} // namespace
