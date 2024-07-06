/*
    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_egl_backend.h"
#include "compositor.h"
#include "core/outputbackend.h"
#include "core/outputlayer.h"
#include "core/overlaywindow.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/glplatform.h"
#include "opengl/glrendertimequery.h"
#include "options.h"
#include "scene/surfaceitem_x11.h"
#include "utils/c_ptr.h"
#include "utils/softwarevsyncmonitor.h"
#include "workspace.h"
#include "x11_standalone_backend.h"
#include "x11_standalone_logging.h"
#include "x11_standalone_overlaywindow.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>

namespace KWin
{

EglLayer::EglLayer(EglBackend *backend)
    : OutputLayer(nullptr)
    , m_backend(backend)
{
}

std::optional<OutputLayerBeginFrameInfo> EglLayer::doBeginFrame()
{
    return m_backend->beginFrame();
}

bool EglLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_backend->endFrame(renderedRegion, damagedRegion, frame);
    return true;
}

DrmDevice *EglLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> EglLayer::supportedDrmFormats() const
{
    return {};
}

EglBackend::EglBackend(::Display *display, X11StandaloneBackend *backend)
    : m_backend(backend)
    , m_overlayWindow(std::make_unique<OverlayWindowX11>(backend))
    , m_layer(std::make_unique<EglLayer>(this))
{
    // There is no any way to determine when a buffer swap completes with EGL. Fallback
    // to software vblank events. Could we use the Present extension to get notified when
    // the overlay window is actually presented on the screen?
    m_vsyncMonitor = SoftwareVsyncMonitor::create();
    connect(backend->renderLoop(), &RenderLoop::refreshRateChanged, this, [this, backend]() {
        m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());
    });
    m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());

    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &EglBackend::vblank);
    Q_ASSERT(workspace());
    connect(workspace(), &Workspace::geometryChanged, this, &EglBackend::screenGeometryChanged);
    overlayWindow()->resize(workspace()->geometry().size());
}

EglBackend::~EglBackend()
{
    m_query.reset();

    if (isFailed() && m_overlayWindow) {
        m_overlayWindow->destroy();
    }
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplayObject()->handle(), m_surface);
    }
    m_context.reset();

    if (m_overlayWindow && m_overlayWindow->window()) {
        m_overlayWindow->destroy();
    }
}

std::unique_ptr<SurfaceTexture> EglBackend::createSurfaceTextureX11(SurfacePixmapX11 *texture)
{
    return std::make_unique<EglSurfaceTextureX11>(this, texture);
}

void EglBackend::init()
{
    QOpenGLContext *qtShareContext = QOpenGLContext::globalShareContext();
    ::EGLDisplay shareDisplay = EGL_NO_DISPLAY;
    ::EGLContext shareContext = EGL_NO_CONTEXT;
    if (qtShareContext) {
        qDebug(KWIN_X11STANDALONE) << "Global share context format:" << qtShareContext->format();
        const auto nativeHandle = qtShareContext->nativeInterface<QNativeInterface::QEGLContext>();
        if (nativeHandle) {
            shareContext = nativeHandle->nativeContext();
            shareDisplay = nativeHandle->display();
        } else {
            setFailed(QStringLiteral("Invalid QOpenGLContext::globalShareContext()"));
            return;
        }
    }
    if (shareContext == EGL_NO_CONTEXT) {
        setFailed(QStringLiteral("QOpenGLContext::globalShareContext() is required"));
        return;
    }

    m_fbo = std::make_unique<GLFramebuffer>(0, workspace()->geometry().size());

    m_backend->setEglDisplay(EglDisplay::create(shareDisplay, false));
    m_backend->setSceneEglGlobalShareContext(shareContext);

    qputenv("EGL_PLATFORM", "x11");
    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    if (!hasExtension(QByteArrayLiteral("EGL_KHR_image")) && (!hasExtension(QByteArrayLiteral("EGL_KHR_image_base")) || !hasExtension(QByteArrayLiteral("EGL_KHR_image_pixmap")))) {
        setFailed(QStringLiteral("Required support for binding pixmaps to EGLImages not found, disabling compositing"));
        return;
    }
    if (!m_context->hasOpenglExtension(QByteArrayLiteral("GL_OES_EGL_image"))) {
        setFailed(QStringLiteral("Required extension GL_OES_EGL_image not found, disabling compositing"));
        return;
    }

    // check for EGL_NV_post_sub_buffer and whether it can be used on the surface
    if (hasExtension(QByteArrayLiteral("EGL_NV_post_sub_buffer"))) {
        if (eglQuerySurface(eglDisplayObject()->handle(), m_surface, EGL_POST_SUB_BUFFER_SUPPORTED_NV, &m_havePostSubBuffer) == EGL_FALSE) {
            EGLint error = eglGetError();
            if (error != EGL_SUCCESS && error != EGL_BAD_ATTRIBUTE) {
                setFailed(QStringLiteral("query surface failed"));
                return;
            } else {
                m_havePostSubBuffer = EGL_FALSE;
            }
        }
    }

    if (m_havePostSubBuffer) {
        qCDebug(KWIN_CORE) << "EGL implementation and surface support eglPostSubBufferNV, let's use it";

        // check if swap interval 1 is supported
        EGLint val;
        eglGetConfigAttrib(eglDisplayObject()->handle(), m_context->config(), EGL_MAX_SWAP_INTERVAL, &val);
        if (val >= 1) {
            if (eglSwapInterval(eglDisplayObject()->handle(), 1)) {
                qCDebug(KWIN_CORE) << "Enabled v-sync";
            }
        } else {
            qCWarning(KWIN_CORE) << "Cannot enable v-sync as max. swap interval is" << val;
        }
    } else {
        /* In the GLX backend, we fall back to using glCopyPixels if we have no extension providing support for partial screen updates.
         * However, that does not work in EGL - glCopyPixels with glDrawBuffer(GL_FRONT); does nothing.
         * Hence we need EGL to preserve the backbuffer for us, so that we can draw the partial updates on it and call
         * eglSwapBuffers() for each frame. eglSwapBuffers() then does the copy (no page flip possible in this mode),
         * which means it is slow and not synced to the v-blank. */
        qCWarning(KWIN_CORE) << "eglPostSubBufferNV not supported, have to enable buffer preservation - which breaks v-sync and performance";
        eglSurfaceAttrib(eglDisplayObject()->handle(), m_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
    }

    m_swapStrategy = options->glPreferBufferSwap();
    if (m_swapStrategy == Options::AutoSwapStrategy) {
        // buffer copying is very fast with the nvidia blob
        // but due to restrictions in DRI2 *incredibly* slow for all MESA drivers
        // see https://www.x.org/releases/X11R7.7/doc/dri2proto/dri2proto.txt, item 2.5
        if (m_context->glPlatform()->driver() == Driver_NVidia) {
            m_swapStrategy = Options::CopyFrontBuffer;
        } else if (m_context->glPlatform()->driver() != Driver_Unknown) { // undetected, finally resolved when context is initialized
            m_swapStrategy = Options::ExtendDamage;
        }
    }
}

void EglBackend::initClientExtensions()
{
    // Get the list of client extensions
    const char *clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        (void)eglGetError();
    }

    m_clientExtensions = clientExtensionsString.split(' ');
}

bool EglBackend::hasClientExtension(const QByteArray &name)
{
    return m_clientExtensions.contains(name);
}

bool EglBackend::initRenderingContext()
{
    initClientExtensions();
    auto display = m_backend->sceneEglDisplayObject();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (!display) {
        m_havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        if (m_havePlatformBase) {
            // Make sure that the X11 platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_x11")) && !hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_x11"))) {
                qCWarning(KWIN_CORE) << "EGL_EXT_platform_base is supported, but neither EGL_EXT_platform_x11 nor EGL_KHR_platform_x11 is supported."
                                     << "Cannot create EGLDisplay on X11";
                return false;
            }

            m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, m_backend->display(), nullptr)));
        } else {
            m_backend->setEglDisplay(EglDisplay::create(eglGetDisplay(m_backend->display())));
        }
        display = m_backend->sceneEglDisplayObject();
        if (!display) {
            qCWarning(KWIN_CORE) << "Failed to get the EGLDisplay";
            return false;
        }
    }

    setSupportsBufferAge(display->supportsBufferAge());
    setSupportsNativeFence(display->supportsNativeFence());
    setExtensions(display->extensions());
    m_context = EglContext::create(display, chooseBufferConfig(), m_backend->sceneEglGlobalShareContext());
    if (!m_context) {
        qCCritical(KWIN_CORE) << "Create OpenGL context failed";
        return false;
    }

    if (!m_overlayWindow->create()) {
        qCCritical(KWIN_X11STANDALONE) << "Could not get overlay window";
        return false;
    } else {
        m_overlayWindow->setup(XCB_WINDOW_NONE);
    }

    m_surface = createSurface(m_overlayWindow->window());
    if (m_surface == EGL_NO_SURFACE) {
        qCCritical(KWIN_CORE) << "Creating egl surface failed";
        return false;
    }

    if (!makeCurrent()) {
        qCCritical(KWIN_CORE) << "Make Context Current failed";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_CORE) << "Error occurred while creating context " << error;
        return false;
    }

    return true;
}

EGLSurface EglBackend::createSurface(xcb_window_t window)
{
    if (window == XCB_WINDOW_NONE) {
        return EGL_NO_SURFACE;
    }

    // Window is 64 bits on a 64-bit architecture whereas xcb_window_t is always 32 bits.
    ::Window nativeWindow = window;

    EGLSurface surface = EGL_NO_SURFACE;
    if (m_havePlatformBase) {
        // eglCreatePlatformWindowSurfaceEXT() expects a pointer to the Window.
        surface = eglCreatePlatformWindowSurfaceEXT(eglDisplayObject()->handle(), m_context->config(), (void *)&nativeWindow, nullptr);
    } else {
        // eglCreateWindowSurface() expects a Window, not a pointer to the Window. Use
        // a c style cast as there are (buggy) platforms where the size of the Window
        // type is not the same as the size of EGLNativeWindowType, reinterpret_cast<>()
        // may not compile.
        surface = eglCreateWindowSurface(eglDisplayObject()->handle(), m_context->config(), (EGLNativeWindowType)(uintptr_t)nativeWindow, nullptr);
    }

    return surface;
}

EGLConfig EglBackend::chooseBufferConfig()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | (supportsBufferAge() ? 0 : EGL_SWAP_BEHAVIOR_PRESERVED_BIT),
        EGL_RED_SIZE,
        1,
        EGL_GREEN_SIZE,
        1,
        EGL_BLUE_SIZE,
        1,
        EGL_ALPHA_SIZE,
        0,
        EGL_RENDERABLE_TYPE,
        EglDisplay::shouldUseOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,
        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(eglDisplayObject()->handle(), config_attribs, configs, 1024, &count) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "choose config failed";
        return EGL_NO_CONFIG_KHR;
    }

    UniqueCPtr<xcb_get_window_attributes_reply_t> attribs(xcb_get_window_attributes_reply(m_backend->connection(),
                                                                                          xcb_get_window_attributes_unchecked(m_backend->connection(), m_backend->rootWindow()),
                                                                                          nullptr));
    if (!attribs) {
        qCCritical(KWIN_CORE) << "Failed to get window attributes of root window";
        return EGL_NO_CONFIG_KHR;
    }

    for (int i = 0; i < count; i++) {
        EGLint val;
        if (eglGetConfigAttrib(eglDisplayObject()->handle(), configs[i], EGL_NATIVE_VISUAL_ID, &val) == EGL_FALSE) {
            qCCritical(KWIN_CORE) << "egl get config attrib failed";
        }
        if (uint32_t(val) == attribs->visual) {
            return configs[i];
        }
    }
    return configs[0];
}

void EglBackend::screenGeometryChanged()
{
    overlayWindow()->resize(workspace()->geometry().size());

    // The back buffer contents are now undefined
    m_bufferAge = 0;
    m_fbo = std::make_unique<GLFramebuffer>(0, workspace()->geometry().size());
}

OutputLayerBeginFrameInfo EglBackend::beginFrame()
{
    makeCurrent();

    QRegion repaint;
    if (supportsBufferAge()) {
        repaint = m_damageJournal.accumulate(m_bufferAge, infiniteRegion());
    }
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    m_query = std::make_unique<GLRenderTimeQuery>(m_context);
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = repaint,
    };
}

void EglBackend::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    // Save the damaged region to history
    if (supportsBufferAge()) {
        m_damageJournal.add(damagedRegion);
    }
    m_lastRenderedRegion = renderedRegion;
}

bool EglBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    m_frame = frame;
    // Start the software vsync monitor. There is no any reliable way to determine when
    // eglSwapBuffers() or eglSwapBuffersWithDamageEXT() completes.
    m_vsyncMonitor->arm();

    QRegion effectiveRenderedRegion = m_lastRenderedRegion;
    if (!m_context->isOpenGLES()) {
        const QRect displayRect = workspace()->geometry();
        if (!supportsBufferAge() && m_swapStrategy == Options::CopyFrontBuffer && m_lastRenderedRegion != displayRect) {
            glReadBuffer(GL_FRONT);
            copyPixels(QRegion(displayRect) - m_lastRenderedRegion, displayRect.size());
            glReadBuffer(GL_BACK);
            effectiveRenderedRegion = displayRect;
        }
    }

    presentSurface(m_surface, effectiveRenderedRegion, workspace()->geometry());

    if (overlayWindow() && overlayWindow()->window()) { // show the window only after the first pass,
        overlayWindow()->show(); // since that pass may take long
    }
    return true;
}

void EglBackend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
{
    const bool fullRepaint = supportsBufferAge() || (damage == screenGeometry);

    if (fullRepaint || !m_havePostSubBuffer) {
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(eglDisplayObject()->handle(), surface);
        if (supportsBufferAge()) {
            eglQuerySurface(eglDisplayObject()->handle(), surface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        }
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        for (const QRect &r : damage) {
            eglPostSubBufferNV(eglDisplayObject()->handle(), surface, r.left(), screenGeometry.height() - r.bottom() - 1, r.width(), r.height());
        }
    }
}

OverlayWindow *EglBackend::overlayWindow() const
{
    return m_overlayWindow.get();
}

OutputLayer *EglBackend::primaryLayer(Output *output)
{
    return m_layer.get();
}

void EglBackend::vblank(std::chrono::nanoseconds timestamp)
{
    m_frame->presented(timestamp, PresentationMode::VSync);
    m_frame.reset();
}

EglDisplay *EglBackend::eglDisplayObject() const
{
    return m_backend->sceneEglDisplayObject();
}

OpenGlContext *EglBackend::openglContext() const
{
    return m_context.get();
}

bool EglBackend::makeCurrent()
{
    return m_context->makeCurrent(m_surface);
}

void EglBackend::doneCurrent()
{
    m_context->doneCurrent();
}

EglSurfaceTextureX11::EglSurfaceTextureX11(EglBackend *backend, SurfacePixmapX11 *texture)
    : OpenGLSurfaceTextureX11(backend, texture)
{
}

bool EglSurfaceTextureX11::create()
{
    auto texture = std::make_shared<EglPixmapTexture>(static_cast<EglBackend *>(m_backend));
    if (texture->create(m_pixmap)) {
        m_texture = {texture};
        return true;
    } else {
        return false;
    }
}

void EglSurfaceTextureX11::update(const QRegion &region)
{
    // mipmaps need to be updated
    m_texture.setDirty();
}

EglPixmapTexture::EglPixmapTexture(EglBackend *backend)
    : GLTexture(GL_TEXTURE_2D)
    , m_backend(backend)
{
}

EglPixmapTexture::~EglPixmapTexture()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->eglDisplayObject()->handle(), m_image);
    }
}

bool EglPixmapTexture::create(SurfacePixmapX11 *pixmap)
{
    const xcb_pixmap_t nativePixmap = pixmap->pixmap();
    if (nativePixmap == XCB_NONE) {
        return false;
    }

    glGenTextures(1, &d->m_texture);
    setWrapMode(GL_CLAMP_TO_EDGE);
    setFilter(GL_LINEAR);
    bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE};
    m_image = eglCreateImageKHR(m_backend->eglDisplayObject()->handle(),
                                EGL_NO_CONTEXT,
                                EGL_NATIVE_PIXMAP_KHR,
                                reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(nativePixmap)),
                                attribs);

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_X11STANDALONE) << "failed to create egl image";
        unbind();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(m_image));
    unbind();
    setContentTransform(OutputTransform::FlipY);
    d->m_size = pixmap->size();
    d->updateMatrix();
    return true;
}

void EglPixmapTexture::onDamage()
{
    if (options->isGlStrictBinding()) {
        // This is just implemented to be consistent with
        // the example in mesa/demos/src/egl/opengles1/texture_from_pixmap.c
        eglWaitNative(EGL_CORE_NATIVE_ENGINE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(m_image));
    }
}

} // namespace KWin

#include "moc_x11_standalone_egl_backend.cpp"
