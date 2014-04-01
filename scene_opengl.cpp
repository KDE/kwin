/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.

Explicit command stream synchronization based on the sample
implementation by James Jones <jajones@nvidia.com>,

Copyright © 2011 NVIDIA Corporation

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "scene_opengl.h"
#ifdef KWIN_HAVE_EGL
#include "eglonxbackend.h"
// for Wayland
#if HAVE_WAYLAND_EGL
#include "egl_wayland_backend.h"
#endif
#endif
#ifndef KWIN_HAVE_OPENGLES
#include "glxbackend.h"
#endif

#include <kwinglcolorcorrection.h>
#include <kwinglplatform.h>

#include "utils.h"
#include "client.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "main.h"
#include "overlaywindow.h"
#include "screens.h"
#include "workspace.h"
#include "decorations/decoratedclient.h"

#include <cmath>
#include <unistd.h>
#include <stddef.h>

#include <qpainter.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QGraphicsScale>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

#include <KLocalizedString>
#include <KNotification>
#include <KProcess>

namespace KWin
{

extern int currentRefreshRate();


/**
 * SyncObject represents a fence used to synchronize operations in
 * the kwin command stream with operations in the X command stream.
 */
class SyncObject
{
public:
    enum State { Ready, TriggerSent, Waiting, Done, Resetting };

    SyncObject();
    ~SyncObject();

    State state() const { return m_state; }

    void trigger();
    void wait();
    bool finish();
    void reset();
    void finishResetting();

private:
    State m_state;
    GLsync m_sync;
    xcb_sync_fence_t m_fence;
    xcb_get_input_focus_cookie_t m_reset_cookie;
};

SyncObject::SyncObject()
{
    m_state = Ready;

    xcb_connection_t * const c = connection();

    m_fence = xcb_generate_id(c);
    xcb_sync_create_fence(c, rootWindow(), m_fence, false);
    xcb_flush(c);

    m_sync = glImportSyncEXT(GL_SYNC_X11_FENCE_EXT, m_fence, 0);
}

SyncObject::~SyncObject()
{
    xcb_sync_destroy_fence(connection(), m_fence);
    glDeleteSync(m_sync);

    if (m_state == Resetting)
        xcb_discard_reply(connection(), m_reset_cookie.sequence);
}

void SyncObject::trigger()
{
    assert(m_state == Ready || m_state == Resetting);

    // Finish resetting the fence if necessary
    if (m_state == Resetting)
        finishResetting();

    xcb_sync_trigger_fence(connection(), m_fence);
    m_state = TriggerSent;
}

void SyncObject::wait()
{
    if (m_state != TriggerSent)
        return;

    glWaitSync(m_sync, 0, GL_TIMEOUT_IGNORED);
    m_state = Waiting;
}

bool SyncObject::finish()
{
    if (m_state == Done)
        return true;

    // Note: It is possible that we never inserted a wait for the fence.
    //       This can happen if we ended up not rendering the damaged
    //       window because it is fully occluded.
    assert(m_state == TriggerSent || m_state == Waiting);

    // Check if the fence is signaled
    GLint value;
    glGetSynciv(m_sync, GL_SYNC_STATUS, 1, nullptr, &value);

    if (value != GL_SIGNALED) {
        qDebug() << "Waiting for X fence to finish";

        // Wait for the fence to become signaled with a one second timeout
        const GLenum result = glClientWaitSync(m_sync, 0, 1000000000);

        switch (result) {
        case GL_TIMEOUT_EXPIRED:
            qWarning() << "Timeout while waiting for X fence";
            return false;

        case GL_WAIT_FAILED:
            qWarning() << "glClientWaitSync() failed";
            return false;
        }
    }

    m_state = Done;
    return true;
}

void SyncObject::reset()
{
    assert(m_state == Done);

    xcb_connection_t * const c = connection();

    // Send the reset request along with a sync request.
    // We use the cookie to ensure that the server has processed the reset
    // request before we trigger the fence and call glWaitSync().
    // Otherwise there is a race condition between the reset finishing and
    // the glWaitSync() call.
    xcb_sync_reset_fence(c, m_fence);
    m_reset_cookie = xcb_get_input_focus(c);
    xcb_flush(c);

    m_state = Resetting;
}

void SyncObject::finishResetting()
{
    assert(m_state == Resetting);
    free(xcb_get_input_focus_reply(connection(), m_reset_cookie, nullptr));
    m_state = Ready;
}



// -----------------------------------------------------------------------



/**
 * SyncManager manages a set of fences used for explicit synchronization
 * with the X command stream.
 */
class SyncManager
{
public:
    enum { MaxFences = 4 };

    SyncManager();
    ~SyncManager();

    SyncObject *nextFence();
    bool updateFences();

private:
    std::array<SyncObject, MaxFences> m_fences;
    int m_next;
};

SyncManager::SyncManager()
    : m_next(0)
{
}

SyncManager::~SyncManager()
{
}

SyncObject *SyncManager::nextFence()
{
    SyncObject *fence = &m_fences[m_next];
    m_next = (m_next + 1) % MaxFences;
    return fence;
}

bool SyncManager::updateFences()
{
    for (int i = 0; i < qMin(2, MaxFences - 1); i++) {
        const int index = (m_next + i) % MaxFences;
        SyncObject &fence = m_fences[index];

        switch (fence.state()) {
        case SyncObject::Ready:
            break;

        case SyncObject::TriggerSent:
        case SyncObject::Waiting:
            if (!fence.finish())
                return false;
            fence.reset();
            break;

        // Should not happen in practice since we always reset the fence
        // after finishing it
        case SyncObject::Done:
            fence.reset();
            break;

        case SyncObject::Resetting:
            fence.finishResetting();
            break;
        }
    }

    return true;
}


// -----------------------------------------------------------------------



//****************************************
// SceneOpenGL
//****************************************
OpenGLBackend::OpenGLBackend()
    : m_syncsToVBlank(false)
    , m_blocksForRetrace(false)
    , m_directRendering(false)
    , m_haveBufferAge(false)
    , m_failed(false)
{
}

OpenGLBackend::~OpenGLBackend()
{
}

void OpenGLBackend::setFailed(const QString &reason)
{
    qWarning() << "Creating the OpenGL rendering failed: " << reason;
    m_failed = true;
}

void OpenGLBackend::idle()
{
    if (hasPendingFlush()) {
        effects->makeOpenGLContextCurrent();
        present();
    }
}

void OpenGLBackend::addToDamageHistory(const QRegion &region)
{
    if (m_damageHistory.count() > 10)
        m_damageHistory.removeLast();

    m_damageHistory.prepend(region);
}

QRegion OpenGLBackend::accumulatedDamageHistory(int bufferAge) const
{
    QRegion region;

    // Note: An age of zero means the buffer contents are undefined
    if (bufferAge > 0 && bufferAge <= m_damageHistory.count()) {
        for (int i = 0; i < bufferAge - 1; i++)
            region |= m_damageHistory[i];
    } else {
        const QSize &s = screens()->size();
        region = QRegion(0, 0, s.width(), s.height());
    }

    return region;
}

OverlayWindow* OpenGLBackend::overlayWindow()
{
    return NULL;
}

/************************************************
 * SceneOpenGL
 ***********************************************/

SceneOpenGL::SceneOpenGL(Workspace* ws, OpenGLBackend *backend)
    : Scene(ws)
    , init_ok(true)
    , m_backend(backend)
    , m_syncManager(nullptr)
    , m_currentFence(nullptr)
{
    if (m_backend->isFailed()) {
        init_ok = false;
        return;
    }
    if (!viewportLimitsMatched(screens()->size()))
        return;

    // perform Scene specific checks
    GLPlatform *glPlatform = GLPlatform::instance();
#ifndef KWIN_HAVE_OPENGLES
    if (!hasGLExtension(QByteArrayLiteral("GL_ARB_texture_non_power_of_two"))
            && !hasGLExtension(QByteArrayLiteral("GL_ARB_texture_rectangle"))) {
        qCritical() << "GL_ARB_texture_non_power_of_two and GL_ARB_texture_rectangle missing";
        init_ok = false;
        return; // error
    }
#endif
    if (glPlatform->isMesaDriver() && glPlatform->mesaVersion() < kVersionNumber(8, 0)) {
        qCritical() << "KWin requires at least Mesa 8.0 for OpenGL compositing.";
        init_ok = false;
        return;
    }
#ifndef KWIN_HAVE_OPENGLES
    glDrawBuffer(GL_BACK);
#endif

    m_debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;
    initDebugOutput();

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!glPlatform->supports(LooseBinding));
    }

    bool haveSyncObjects = glPlatform->isGLES()
        ? hasGLVersion(3, 0)
        : hasGLVersion(3, 2) || hasGLExtension("GL_ARB_sync");

    if (hasGLExtension("GL_EXT_x11_sync_object") && haveSyncObjects) {
        const QByteArray useExplicitSync = qgetenv("KWIN_EXPLICIT_SYNC");

        if (useExplicitSync != "0") {
            qDebug() << "Initializing fences for synchronization with the X command stream";
            m_syncManager = new SyncManager;
        } else {
            qDebug() << "Explicit synchronization with the X command stream disabled by environment variable";
        }
    }
}

SceneOpenGL::~SceneOpenGL()
{
    // do cleanup after initBuffer()
    SceneOpenGL::EffectFrame::cleanup();
    if (init_ok) {
        delete m_syncManager;

        // backend might be still needed for a different scene
        delete m_backend;
    }
}

void SceneOpenGL::initDebugOutput()
{
    const bool have_KHR_debug = hasGLExtension(QByteArrayLiteral("GL_KHR_debug"));
    if (!have_KHR_debug && !hasGLExtension(QByteArrayLiteral("GL_ARB_debug_output")))
        return;

    // Set the callback function
    auto callback = [](GLenum source, GLenum type, GLuint id,
                       GLenum severity, GLsizei length,
                       const GLchar *message,
                       const GLvoid *userParam) {
        Q_UNUSED(source)
        Q_UNUSED(severity)
        Q_UNUSED(userParam)

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            qWarning("%#x: %.*s", id, length, message);
            break;

        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        case GL_DEBUG_TYPE_OTHER:
        default:
            qDebug("%#x: %.*s", id, length, message);
            break;
        }
    };

    glDebugMessageCallback(callback, nullptr);

    // This state exists only in GL_KHR_debug
    if (have_KHR_debug)
        glEnable(GL_DEBUG_OUTPUT);

#ifndef NDEBUG
    // Enable all debug messages
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#else
    // Enable error messages
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

    // Insert a test message
    const QByteArray message = QByteArrayLiteral("OpenGL debug output initialized");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0,
                         GL_DEBUG_SEVERITY_LOW, message.length(), message.constData());
}

SceneOpenGL *SceneOpenGL::createScene()
{
    OpenGLBackend *backend = NULL;
    OpenGLPlatformInterface platformInterface = options->glPlatformInterface();

    switch (platformInterface) {
    case GlxPlatformInterface:
#ifndef KWIN_HAVE_OPENGLES
        backend = new GlxBackend();
#endif
        break;
    case EglPlatformInterface:
#ifdef KWIN_HAVE_EGL
#if HAVE_WAYLAND_EGL
        if (kwinApp()->shouldUseWaylandForCompositing()) {
            backend = new EglWaylandBackend();
        } else {
            backend = new EglOnXBackend();
        }
#else
        backend = new EglOnXBackend();
#endif
#endif
        break;
    default:
        // no backend available
        return NULL;
    }
    if (!backend || backend->isFailed()) {
        delete backend;
        return NULL;
    }
    SceneOpenGL *scene = NULL;
    // first let's try an OpenGL 2 scene
    if (SceneOpenGL2::supported(backend)) {
        scene = new SceneOpenGL2(backend);
        if (scene->initFailed()) {
            delete scene;
            scene = NULL;
        } else {
            return scene;
        }
    }
    if (!scene) {
        if (GLPlatform::instance()->recommendedCompositor() == XRenderCompositing) {
            qCritical() << "OpenGL driver recommends XRender based compositing. Falling back to XRender.";
            qCritical() << "To overwrite the detection use the environment variable KWIN_COMPOSE";
            qCritical() << "For more information see http://community.kde.org/KWin/Environment_Variables#KWIN_COMPOSE";
            QTimer::singleShot(0, Compositor::self(), SLOT(fallbackToXRenderCompositing()));
        }
        delete backend;
    }

    return scene;
}

OverlayWindow *SceneOpenGL::overlayWindow()
{
    return m_backend->overlayWindow();
}

bool SceneOpenGL::syncsToVBlank() const
{
    return m_backend->syncsToVBlank();
}

bool SceneOpenGL::blocksForRetrace() const
{
    return m_backend->blocksForRetrace();
}

void SceneOpenGL::idle()
{
    m_backend->idle();
    Scene::idle();
}

bool SceneOpenGL::initFailed() const
{
    return !init_ok;
}

#ifndef KWIN_HAVE_OPENGLES
void SceneOpenGL::copyPixels(const QRegion &region)
{
    const int height = screens()->size().height();
    foreach (const QRect &r, region.rects()) {
        const int x0 = r.x();
        const int y0 = height - r.y() - r.height();
        const int x1 = r.x() + r.width();
        const int y1 = height - r.y();

        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}
#endif

#ifndef KWIN_HAVE_OPENGLES
#  define GL_GUILTY_CONTEXT_RESET_KWIN    GL_GUILTY_CONTEXT_RESET_ARB
#  define GL_INNOCENT_CONTEXT_RESET_KWIN  GL_INNOCENT_CONTEXT_RESET_ARB
#  define GL_UNKNOWN_CONTEXT_RESET_KWIN   GL_UNKNOWN_CONTEXT_RESET_ARB
#else
#  define GL_GUILTY_CONTEXT_RESET_KWIN    GL_GUILTY_CONTEXT_RESET_EXT
#  define GL_INNOCENT_CONTEXT_RESET_KWIN  GL_INNOCENT_CONTEXT_RESET_EXT
#  define GL_UNKNOWN_CONTEXT_RESET_KWIN   GL_UNKNOWN_CONTEXT_RESET_EXT
#endif

void SceneOpenGL::handleGraphicsReset(GLenum status)
{
    switch (status) {
    case GL_GUILTY_CONTEXT_RESET_KWIN:
        qDebug() << "A graphics reset attributable to the current GL context occurred.";
        break;

    case GL_INNOCENT_CONTEXT_RESET_KWIN:
        qDebug() << "A graphics reset not attributable to the current GL context occurred.";
        break;

    case GL_UNKNOWN_CONTEXT_RESET_KWIN:
        qDebug() << "A graphics reset of an unknown cause occurred.";
        break;

    default:
        break;
    }

    QElapsedTimer timer;
    timer.start();

    // Wait until the reset is completed or max 10 seconds
    while (timer.elapsed() < 10000 && glGetGraphicsResetStatus() != GL_NO_ERROR)
        usleep(50);

    qDebug() << "Attempting to reset compositing.";
    QMetaObject::invokeMethod(this, "resetCompositing", Qt::QueuedConnection);

    KNotification::event(QStringLiteral("graphicsreset"), i18n("Desktop effects were restarted due to a graphics reset"));
}


void SceneOpenGL::triggerFence()
{
    if (m_syncManager) {
        m_currentFence = m_syncManager->nextFence();
        m_currentFence->trigger();
    }
}

void SceneOpenGL::insertWait()
{
    if (m_currentFence && m_currentFence->state() != SyncObject::Waiting) {
        m_currentFence->wait();
    }
}

qint64 SceneOpenGL::paint(QRegion damage, ToplevelList toplevels)
{
    // actually paint the frame, flushed with the NEXT frame
    createStackingOrder(toplevels);

    m_backend->makeCurrent();
    QRegion repaint = m_backend->prepareRenderingFrame();

    const GLenum status = glGetGraphicsResetStatus();
    if (status != GL_NO_ERROR) {
        handleGraphicsReset(status);
        return 0;
    }

    int mask = 0;

    // After this call, updateRegion will contain the damaged region in the
    // back buffer. This is the region that needs to be posted to repair
    // the front buffer. It doesn't include the additional damage returned
    // by prepareRenderingFrame(). validRegion is the region that has been
    // repainted, and may be larger than updateRegion.
    QRegion updateRegion, validRegion;
    paintScreen(&mask, damage, repaint, &updateRegion, &validRegion);   // call generic implementation

#ifndef KWIN_HAVE_OPENGLES
    const QSize &screenSize = screens()->size();
    const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());

    // copy dirty parts from front to backbuffer
    if (!m_backend->supportsBufferAge() &&
        options->glPreferBufferSwap() == Options::CopyFrontBuffer &&
        validRegion != displayRegion) {
        glReadBuffer(GL_FRONT);
        copyPixels(displayRegion - validRegion);
        glReadBuffer(GL_BACK);
        validRegion = displayRegion;
    }
#endif

    GLVertexBuffer::streamingBuffer()->endOfFrame();

    m_backend->endRenderingFrame(validRegion, updateRegion);

    GLVertexBuffer::streamingBuffer()->framePosted();

    if (m_currentFence) {
        if (!m_syncManager->updateFences()) {
            qDebug() << "Aborting explicit synchronization with the X command stream.";
            qDebug() << "Future frames will be rendered unsynchronized.";
            delete m_syncManager;
            m_syncManager = nullptr;
        }
        m_currentFence = nullptr;
    }

    // do cleanup
    clearStackingOrder();
    return m_backend->renderTime();
}

QMatrix4x4 SceneOpenGL::transformation(int mask, const ScreenPaintData &data) const
{
    QMatrix4x4 matrix;

    if (!(mask & PAINT_SCREEN_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // cannot use data.rotation->applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}

void SceneOpenGL::paintBackground(QRegion region)
{
    PaintClipper pc(region);
    if (!PaintClipper::clip()) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    if (pc.clip() && pc.paintArea().isEmpty())
        return; // no background to paint
    QVector<float> verts;
    for (PaintClipper::Iterator iterator; !iterator.isDone(); iterator.next()) {
        QRect r = iterator.boundingRect();
        verts << r.x() + r.width() << r.y();
        verts << r.x() << r.y();
        verts << r.x() << r.y() + r.height();
        verts << r.x() << r.y() + r.height();
        verts << r.x() + r.width() << r.y() + r.height();
        verts << r.x() + r.width() << r.y();
    }
    doPaintBackground(verts);
}

void SceneOpenGL::extendPaintRegion(QRegion &region, bool opaqueFullscreen)
{
    if (m_backend->supportsBufferAge())
        return;

    const QSize &screenSize = screens()->size();
    if (options->glPreferBufferSwap() == Options::ExtendDamage) { // only Extend "large" repaints
        const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());
        uint damagedPixels = 0;
        const uint fullRepaintLimit = (opaqueFullscreen?0.49f:0.748f)*screenSize.width()*screenSize.height();
        // 16:9 is 75% of 4:3 and 2.55:1 is 49.01% of 5:4
        // (5:4 is the most square format and 2.55:1 is Cinemascope55 - the widest ever shot
        // movie aspect - two times ;-) It's a Fox format, though, so maybe we want to restrict
        // to 2.20:1 - Panavision - which has actually been used for interesting movies ...)
        // would be 57% of 5/4
        foreach (const QRect &r, region.rects()) {
//                 damagedPixels += r.width() * r.height(); // combined window damage test
            damagedPixels = r.width() * r.height(); // experimental single window damage testing
            if (damagedPixels > fullRepaintLimit) {
                region = displayRegion;
                return;
            }
        }
    } else if (options->glPreferBufferSwap() == Options::PaintFullScreen) { // forced full rePaint
        region = QRegion(0, 0, screenSize.width(), screenSize.height());
    }
}

SceneOpenGL::Texture *SceneOpenGL::createTexture()
{
    return new Texture(m_backend);
}

bool SceneOpenGL::viewportLimitsMatched(const QSize &size) const {
    GLint limit[2];
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, limit);
    if (limit[0] < size.width() || limit[1] < size.height()) {
        QMetaObject::invokeMethod(Compositor::self(), "suspend",
                                  Qt::QueuedConnection, Q_ARG(Compositor::SuspendReason, Compositor::AllReasonSuspend));
        const QString message = i18n("<h1>OpenGL desktop effects not possible</h1>"
                                     "Your system cannot perform OpenGL Desktop Effects at the "
                                     "current resolution<br><br>"
                                     "You can try to select the XRender backend, but it "
                                     "might be very slow for this resolution as well.<br>"
                                     "Alternatively, lower the combined resolution of all screens "
                                     "to %1x%2 ", limit[0], limit[1]);
        const QString details = i18n("The demanded resolution exceeds the GL_MAX_VIEWPORT_DIMS "
                                     "limitation of your GPU and is therefore not compatible "
                                     "with the OpenGL compositor.<br>"
                                     "XRender does not know such limitation, but the performance "
                                     "will usually be impacted by the hardware limitations that "
                                     "restrict the OpenGL viewport size.");
        const int oldTimeout = QDBusConnection::sessionBus().interface()->timeout();
        QDBusConnection::sessionBus().interface()->setTimeout(500);
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kwinCompositingDialog")).value()) {
            QDBusInterface dialog( QStringLiteral("org.kde.kwinCompositingDialog"), QStringLiteral("/CompositorSettings"), QStringLiteral("org.kde.kwinCompositingDialog") );
            dialog.asyncCall(QStringLiteral("warn"), message, details, QString());
        } else {
            const QString args = QStringLiteral("warn ") + QString::fromUtf8(message.toLocal8Bit().toBase64()) + QStringLiteral(" details ") + QString::fromUtf8(details.toLocal8Bit().toBase64());
            KProcess::startDetached(QStringLiteral("kcmshell5"), QStringList() << QStringLiteral("kwincompositing") << QStringLiteral("--args") << args);
        }
        QDBusConnection::sessionBus().interface()->setTimeout(oldTimeout);
        return false;
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, limit);
    if (limit[0] < size.width() || limit[0] < size.height()) {
        KConfig cfg(QStringLiteral("kwin_dialogsrc"));

        if (!KConfigGroup(&cfg, "Notification Messages").readEntry("max_tex_warning", true))
            return true;

        const QString message = i18n("<h1>OpenGL desktop effects might be unusable</h1>"
                                     "OpenGL Desktop Effects at the current resolution are supported "
                                     "but might be exceptionally slow.<br>"
                                     "Also large windows will turn entirely black.<br><br>"
                                     "Consider to suspend compositing, switch to the XRender backend "
                                     "or lower the resolution to %1x%1." , limit[0]);
        const QString details = i18n("The demanded resolution exceeds the GL_MAX_TEXTURE_SIZE "
                                     "limitation of your GPU, thus windows of that size cannot be "
                                     "assigned to textures and will be entirely black.<br>"
                                     "Also this limit will often be a performance level barrier despite "
                                     "below GL_MAX_VIEWPORT_DIMS, because the driver might fall back to "
                                     "software rendering in this case.");
        const int oldTimeout = QDBusConnection::sessionBus().interface()->timeout();
        QDBusConnection::sessionBus().interface()->setTimeout(500);
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kwinCompositingDialog")).value()) {
            QDBusInterface dialog( QStringLiteral("org.kde.kwinCompositingDialog"), QStringLiteral("/CompositorSettings"), QStringLiteral("org.kde.kwinCompositingDialog") );
            dialog.asyncCall(QStringLiteral("warn"), message, details, QStringLiteral("kwin_dialogsrc:max_tex_warning"));
        } else {
            const QString args = QStringLiteral("warn ") + QString::fromUtf8(message.toLocal8Bit().toBase64()) + QStringLiteral(" details ") +
                                 QString::fromUtf8(details.toLocal8Bit().toBase64()) + QStringLiteral(" dontagain kwin_dialogsrc:max_tex_warning");
            KProcess::startDetached(QStringLiteral("kcmshell5"), QStringList() << QStringLiteral("kwincompositing") << QStringLiteral("--args") << args);
        }
        QDBusConnection::sessionBus().interface()->setTimeout(oldTimeout);
    }
    return true;
}

void SceneOpenGL::screenGeometryChanged(const QSize &size)
{
    if (!viewportLimitsMatched(size))
        return;
    Scene::screenGeometryChanged(size);
    glViewport(0,0, size.width(), size.height());
    m_backend->screenGeometryChanged(size);
    ShaderManager::setVirtualScreenSize(size);
    GLRenderTarget::setVirtualScreenSize(size);
    GLVertexBuffer::setVirtualScreenSize(size);
    ShaderManager::instance()->resetAllShaders();
}

void SceneOpenGL::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    const QRect r = region.boundingRect();
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x(), screens()->size().height() - r.y() - r.height(), r.width(), r.height());
    KWin::Scene::paintDesktop(desktop, mask, region, data);
    glDisable(GL_SCISSOR_TEST);
}

bool SceneOpenGL::makeOpenGLContextCurrent()
{
    return m_backend->makeCurrent();
}

void SceneOpenGL::doneOpenGLContextCurrent()
{
    m_backend->doneCurrent();
}

Scene::EffectFrame *SceneOpenGL::createEffectFrame(EffectFrameImpl *frame)
{
    return new SceneOpenGL::EffectFrame(frame, this);
}

Shadow *SceneOpenGL::createShadow(Toplevel *toplevel)
{
    return new SceneOpenGLShadow(toplevel);
}

Decoration::Renderer *SceneOpenGL::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneOpenGLDecorationRenderer(impl);
}

//****************************************
// SceneOpenGL2
//****************************************
bool SceneOpenGL2::supported(OpenGLBackend *backend)
{
    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O2") == 0) {
            qDebug() << "OpenGL 2 compositing enforced by environment variable";
            return true;
        } else {
            // OpenGL 2 disabled by environment variable
            return false;
        }
    }
    if (!backend->isDirectRendering()) {
        return false;
    }
    if (GLPlatform::instance()->recommendedCompositor() < OpenGL2Compositing) {
        qDebug() << "Driver does not recommend OpenGL 2 compositing";
#ifndef KWIN_HAVE_OPENGLES
        return false;
#endif
    }
    return true;
}

SceneOpenGL2::SceneOpenGL2(OpenGLBackend *backend)
    : SceneOpenGL(Workspace::self(), backend)
    , m_lanczosFilter(NULL)
    , m_colorCorrection()
{
    if (!init_ok) {
        // base ctor already failed
        return;
    }
    // Initialize color correction before the shaders
    slotColorCorrectedChanged(false);
    connect(options, SIGNAL(colorCorrectedChanged()), this, SLOT(slotColorCorrectedChanged()), Qt::QueuedConnection);

    const QSize &s = screens()->size();
    ShaderManager::setVirtualScreenSize(s);
    GLRenderTarget::setVirtualScreenSize(s);
    GLVertexBuffer::setVirtualScreenSize(s);
    if (!ShaderManager::instance()->isValid()) {
        qDebug() << "No Scene Shaders available";
        init_ok = false;
        return;
    }

    // push one shader on the stack so that one is always bound
    ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
    if (checkGLError("Init")) {
        qCritical() << "OpenGL 2 compositing setup failed";
        init_ok = false;
        return; // error
    }

    qDebug() << "OpenGL 2 compositing successfully initialized";

#ifndef KWIN_HAVE_OPENGLES
    // It is not legal to not have a vertex array object bound in a core context
    if (hasGLExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }
#endif

    init_ok = true;
}

SceneOpenGL2::~SceneOpenGL2()
{
}

QMatrix4x4 SceneOpenGL2::createProjectionMatrix() const
{
    // Create a perspective projection with a 60° field-of-view,
    // and an aspect ratio of 1.0.
    const float fovY   =   60.0f;
    const float aspect =    1.0f;
    const float zNear  =    0.1f;
    const float zFar   =  100.0f;

    const float yMax   =  zNear * std::tan(fovY * M_PI / 360.0f);
    const float yMin   = -yMax;
    const float xMin   =  yMin * aspect;
    const float xMax   =  yMax * aspect;

    QMatrix4x4 projection;
    projection.frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    // Create a second matrix that transforms screen coordinates
    // to world coordinates.
    const float scaleFactor = 1.1 * std::tan(fovY * M_PI / 360.0f) / yMax;
    const QSize size = screens()->size();

    QMatrix4x4 matrix;
    matrix.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    matrix.scale( (xMax - xMin) * scaleFactor / size.width(),
                 -(yMax - yMin) * scaleFactor / size.height(),
                  0.001);

    // Combine the matrices
    return projection * matrix;
}

void SceneOpenGL2::paintSimpleScreen(int mask, QRegion region)
{
    m_projectionMatrix = createProjectionMatrix();
    m_screenProjectionMatrix = m_projectionMatrix;

    Scene::paintSimpleScreen(mask, region);
}

void SceneOpenGL2::paintGenericScreen(int mask, ScreenPaintData data)
{
    const QMatrix4x4 screenMatrix = transformation(mask, data);
    const QMatrix4x4 pMatrix = createProjectionMatrix();

    m_projectionMatrix = pMatrix;
    m_screenProjectionMatrix = pMatrix * screenMatrix;

    // ### Remove the following two lines when there are no more users of the old shader API
    ShaderBinder binder(ShaderManager::GenericShader);
    binder.shader()->setUniform(GLShader::ScreenTransformation, screenMatrix);

    Scene::paintGenericScreen(mask, data);
}

void SceneOpenGL2::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    ShaderBinder binder(ShaderManager::GenericShader);
    GLShader *shader = binder.shader();
    QMatrix4x4 screenTransformation = shader->getUniformMatrix4x4("screenTransformation");

    KWin::SceneOpenGL::paintDesktop(desktop, mask, region, data);

    shader->setUniform(GLShader::ScreenTransformation, screenTransformation);
}

void SceneOpenGL2::doPaintBackground(const QVector< float >& vertices)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setData(vertices.count() / 2, 2, vertices.data(), NULL);

    ShaderBinder binder(ShaderManager::ColorShader);
    binder.shader()->setUniform(GLShader::Offset, QVector2D(0, 0));

    vbo->render(GL_TRIANGLES);
}

Scene::Window *SceneOpenGL2::createWindow(Toplevel *t)
{
    SceneOpenGL2Window *w = new SceneOpenGL2Window(t);
    w->setScene(this);
    return w;
}

void SceneOpenGL2::finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (!m_colorCorrection.isNull() && m_colorCorrection->isEnabled()) {
        // Split the painting for separate screens
        const int numScreens = screens()->count();
        for (int screen = 0; screen < numScreens; ++ screen) {
            QRegion regionForScreen(region);
            if (numScreens > 1)
                regionForScreen = region.intersected(screens()->geometry(screen));

            data.setScreen(screen);
            performPaintWindow(w, mask, regionForScreen, data);
        }
    } else {
        performPaintWindow(w, mask, region, data);
    }
}

void SceneOpenGL2::performPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (mask & PAINT_WINDOW_LANCZOS) {
        if (!m_lanczosFilter) {
            m_lanczosFilter = new LanczosFilter(this);
            // recreate the lanczos filter when the screen gets resized
            connect(screens(), SIGNAL(changed()), SLOT(resetLanczosFilter()));
        }
        m_lanczosFilter->performPaint(w, mask, region, data);
    } else
        w->sceneWindow()->performPaint(mask, region, data);
}

void SceneOpenGL2::resetLanczosFilter()
{
    // TODO: Qt5 - replace by a lambda slot
    delete m_lanczosFilter;
    m_lanczosFilter = NULL;
}

ColorCorrection *SceneOpenGL2::colorCorrection()
{
    return m_colorCorrection.data();
}

void SceneOpenGL2::slotColorCorrectedChanged(bool recreateShaders)
{
    qDebug() << "Color correction:" << options->isColorCorrected();
    if (options->isColorCorrected() && m_colorCorrection.isNull()) {
        m_colorCorrection.reset(new ColorCorrection(this));
        if (!m_colorCorrection->setEnabled(true)) {
            m_colorCorrection.reset();
            return;
        }
        connect(m_colorCorrection.data(), SIGNAL(changed()), Compositor::self(), SLOT(addRepaintFull()));
        connect(m_colorCorrection.data(), SIGNAL(errorOccured()), options, SLOT(setColorCorrected()), Qt::QueuedConnection);
        if (recreateShaders) {
            // Reload all shaders
            ShaderManager::cleanup();
            ShaderManager::instance();
        }
    } else {
        m_colorCorrection.reset();
    }
    Compositor::self()->addRepaintFull();
}

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::Texture::Texture(OpenGLBackend *backend)
    : GLTexture(*backend->createBackendTexture(this))
{
}

SceneOpenGL::Texture::~Texture()
{
}

SceneOpenGL::Texture& SceneOpenGL::Texture::operator = (const SceneOpenGL::Texture& tex)
{
    d_ptr = tex.d_ptr;
    return *this;
}

void SceneOpenGL::Texture::discard()
{
    d_ptr = d_func()->backend()->createBackendTexture(this);
}

bool SceneOpenGL::Texture::load(xcb_pixmap_t pix, const QSize &size,
                                xcb_visualid_t visual)
{
    if (pix == XCB_NONE)
        return false;

    // decrease the reference counter for the old texture
    d_ptr = d_func()->backend()->createBackendTexture(this); //new TexturePrivate();

    Q_D(Texture);
    return d->loadTexture(pix, size, visual);
}

//****************************************
// SceneOpenGL::Texture
//****************************************
SceneOpenGL::TexturePrivate::TexturePrivate()
{
}

SceneOpenGL::TexturePrivate::~TexturePrivate()
{
}

//****************************************
// SceneOpenGL::Window
//****************************************

SceneOpenGL::Window::Window(Toplevel* c)
    : Scene::Window(c)
    , m_scene(NULL)
{
}

SceneOpenGL::Window::~Window()
{
}

static SceneOpenGL::Texture *s_frameTexture = NULL;
// Bind the window pixmap to an OpenGL texture.
bool SceneOpenGL::Window::bindTexture()
{
    s_frameTexture = NULL;
    OpenGLWindowPixmap *pixmap = windowPixmap<OpenGLWindowPixmap>();
    if (!pixmap) {
        return false;
    }
    s_frameTexture = pixmap->texture();
    if (pixmap->isDiscarded()) {
        return !pixmap->texture()->isNull();
    }

    if (!window()->damage().isEmpty())
        m_scene->insertWait();

    return pixmap->bind();
}

QMatrix4x4 SceneOpenGL::Window::transformation(int mask, const WindowPaintData &data) const
{
    QMatrix4x4 matrix;
    matrix.translate(x(), y());

    if (!(mask & PAINT_WINDOW_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // cannot use data.rotation.applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}

bool SceneOpenGL::Window::beginRenderWindow(int mask, const QRegion &region, WindowPaintData &data)
{
    if (region.isEmpty())
        return false;

    m_hardwareClipping = region != infiniteRegion() && (mask & PAINT_WINDOW_TRANSFORMED) && !(mask & PAINT_SCREEN_TRANSFORMED);
    if (region != infiniteRegion() && !m_hardwareClipping) {
        WindowQuadList quads;
        quads.reserve(data.quads.count());

        const QRegion filterRegion = region.translated(-x(), -y());
        // split all quads in bounding rect with the actual rects in the region
        foreach (const WindowQuad &quad, data.quads) {
            foreach (const QRect &r, filterRegion.rects()) {
                const QRectF rf(r);
                const QRectF quadRect(QPointF(quad.left(), quad.top()), QPointF(quad.right(), quad.bottom()));
                const QRectF &intersected = rf.intersected(quadRect);
                if (intersected.isValid()) {
                    if (quadRect == intersected) {
                        // case 1: completely contains, include and do not check other rects
                        quads << quad;
                        break;
                    }
                    // case 2: intersection
                    quads << quad.makeSubQuad(intersected.left(), intersected.top(), intersected.right(), intersected.bottom());
                }
            }
        }
        data.quads = quads;
    }

    if (data.quads.isEmpty())
        return false;

    if (!bindTexture() || !s_frameTexture) {
        return false;
    }

    if (m_hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    // Update the texture filter
    if (options->glSmoothScale() != 0 &&
        (mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        filter = ImageFilterGood;
    else
        filter = ImageFilterFast;

    s_frameTexture->setFilter(filter == ImageFilterGood ? GL_LINEAR : GL_NEAREST);

    const GLVertexAttrib attribs[] = {
        { VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position) },
        { VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord) },
    };

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));

    return true;
}

void SceneOpenGL::Window::endRenderWindow()
{
    if (m_hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }
}

GLTexture *SceneOpenGL::Window::getDecorationTexture() const
{
    if (toplevel->isClient()) {
        Client *client = static_cast<Client *>(toplevel);
        if (client->noBorder()) {
            return nullptr;
        }

        if (!client->isDecorated()) {
            return nullptr;
        }
        if (SceneOpenGLDecorationRenderer *renderer = static_cast<SceneOpenGLDecorationRenderer*>(client->decoratedClient()->renderer())) {
            renderer->render();
            return renderer->texture();
        }
    } else if (toplevel->isDeleted()) {
        Deleted *deleted = static_cast<Deleted *>(toplevel);
        if (!deleted->wasClient() || deleted->noBorder()) {
            return nullptr;
        }
        if (const SceneOpenGLDecorationRenderer *renderer = static_cast<const SceneOpenGLDecorationRenderer*>(deleted->decorationRenderer())) {
            return renderer->texture();
        }
    }
    return nullptr;
}

WindowPixmap* SceneOpenGL::Window::createWindowPixmap()
{
    return new OpenGLWindowPixmap(this, m_scene);
}

//***************************************
// SceneOpenGL2Window
//***************************************
SceneOpenGL2Window::SceneOpenGL2Window(Toplevel *c)
    : SceneOpenGL::Window(c)
    , m_blendingEnabled(false)
{
}

SceneOpenGL2Window::~SceneOpenGL2Window()
{
}

QVector4D SceneOpenGL2Window::modulate(float opacity, float brightness) const
{
    const float a = opacity;
    const float rgb = opacity * brightness;

    return QVector4D(rgb, rgb, rgb, a);
}

void SceneOpenGL2Window::setBlendEnabled(bool enabled)
{
    if (enabled && !m_blendingEnabled)
        glEnable(GL_BLEND);
    else if (!enabled && m_blendingEnabled)
        glDisable(GL_BLEND);

    m_blendingEnabled = enabled;
}

void SceneOpenGL2Window::setupLeafNodes(LeafNode *nodes, const WindowQuadList *quads, const WindowPaintData &data)
{
    if (!quads[ShadowLeaf].isEmpty()) {
        nodes[ShadowLeaf].texture = static_cast<SceneOpenGLShadow *>(m_shadow)->shadowTexture();
        nodes[ShadowLeaf].opacity = data.opacity();
        nodes[ShadowLeaf].hasAlpha = true;
        nodes[ShadowLeaf].coordinateType = NormalizedCoordinates;
    }

    if (!quads[DecorationLeaf].isEmpty()) {
        nodes[DecorationLeaf].texture = getDecorationTexture();
        nodes[DecorationLeaf].opacity = data.opacity();
        nodes[DecorationLeaf].hasAlpha = true;
        nodes[DecorationLeaf].coordinateType = UnnormalizedCoordinates;
    }

    nodes[ContentLeaf].texture = s_frameTexture;
    nodes[ContentLeaf].hasAlpha = !isOpaque();
    // TODO: ARGB crsoofading is atm. a hack, playing on opacities for two dumb SrcOver operations
    // Should be a shader
    if (data.crossFadeProgress() != 1.0 && (data.opacity() < 0.95 || toplevel->hasAlpha())) {
        const float opacity = 1.0 - data.crossFadeProgress();
        nodes[ContentLeaf].opacity = data.opacity() * (1 - pow(opacity, 1.0f + 2.0f * data.opacity()));
    } else {
        nodes[ContentLeaf].opacity = data.opacity();
    }
    nodes[ContentLeaf].coordinateType = UnnormalizedCoordinates;

    if (data.crossFadeProgress() != 1.0) {
        OpenGLWindowPixmap *previous = previousWindowPixmap<OpenGLWindowPixmap>();
        nodes[PreviousContentLeaf].texture = previous ? previous->texture() : NULL;
        nodes[PreviousContentLeaf].hasAlpha = !isOpaque();
        nodes[PreviousContentLeaf].opacity = data.opacity() * (1.0 - data.crossFadeProgress());
        nodes[PreviousContentLeaf].coordinateType = NormalizedCoordinates;
    }
}

QMatrix4x4 SceneOpenGL2Window::modelViewProjectionMatrix(int mask, const WindowPaintData &data) const
{
    SceneOpenGL2 *scene = static_cast<SceneOpenGL2 *>(m_scene);

    const QMatrix4x4 pMatrix = data.projectionMatrix();
    const QMatrix4x4 mvMatrix = data.modelViewMatrix();

    // An effect may want to override the default projection matrix in some cases,
    // such as when it is rendering a window on a render target that doesn't have
    // the same dimensions as the default framebuffer.
    //
    // Note that the screen transformation is not applied here.
    if (!pMatrix.isIdentity())
        return pMatrix * mvMatrix;

    // If an effect has specified a model-view matrix, we multiply that matrix
    // with the default projection matrix.  If the effect hasn't specified a
    // model-view matrix, mvMatrix will be the identity matrix.
    if (mask & Scene::PAINT_SCREEN_TRANSFORMED)
        return scene->screenProjectionMatrix() * mvMatrix;

    return scene->projectionMatrix() * mvMatrix;
}

void SceneOpenGL2Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (!beginRenderWindow(mask, region, data))
        return;

    SceneOpenGL2 *scene = static_cast<SceneOpenGL2 *>(m_scene);

    const QMatrix4x4 windowMatrix = transformation(mask, data);
    const QMatrix4x4 mvpMatrix = modelViewProjectionMatrix(mask, data) * windowMatrix;

    GLShader *shader = data.shader;
    if (!shader) {
        ShaderTraits traits = ShaderTrait::MapTexture;

        if (data.opacity() != 1.0 || data.brightness() != 1.0)
            traits |= ShaderTrait::Modulate;

        if (data.saturation() != 1.0)
            traits |= ShaderTrait::AdjustSaturation;

        shader = ShaderManager::instance()->pushShader(traits);
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvpMatrix);
    }

    if (ColorCorrection *cc = scene->colorCorrection()) {
        cc->setupForOutput(data.screen());
    }

    // ### Remove the following line when there are no more users of the old shader API
    shader->setUniform(GLShader::WindowTransformation, windowMatrix);
    shader->setUniform(GLShader::Saturation, data.saturation());

    const GLenum filter = (mask & (Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_SCREEN_TRANSFORMED))
                           && options->glSmoothScale() != 0 ? GL_LINEAR : GL_NEAREST;

    WindowQuadList quads[LeafCount];

    // Split the quads into separate lists for each type
    foreach (const WindowQuad &quad, data.quads) {
        switch (quad.type()) {
        case WindowQuadDecoration:
            quads[DecorationLeaf].append(quad);
            continue;

        case WindowQuadContents:
            quads[ContentLeaf].append(quad);
            continue;

        case WindowQuadShadow:
            quads[ShadowLeaf].append(quad);
            continue;

        default:
            continue;
        }
    }

    if (data.crossFadeProgress() != 1.0) {
        OpenGLWindowPixmap *previous = previousWindowPixmap<OpenGLWindowPixmap>();
        if (previous) {
            const QRect &oldGeometry = previous->contentsRect();
            for (const WindowQuad &quad : quads[ContentLeaf]) {
                // we need to create new window quads with normalize texture coordinates
                // normal quads divide the x/y position by width/height. This would not work as the texture
                // is larger than the visible content in case of a decorated Client resulting in garbage being shown.
                // So we calculate the normalized texture coordinate in the Client's new content space and map it to
                // the previous Client's content space.
                WindowQuad newQuad(WindowQuadContents);
                for (int i = 0; i < 4; ++i) {
                    const qreal xFactor = qreal(quad[i].textureX() - toplevel->clientPos().x())/qreal(toplevel->clientSize().width());
                    const qreal yFactor = qreal(quad[i].textureY() - toplevel->clientPos().y())/qreal(toplevel->clientSize().height());
                    WindowVertex vertex(quad[i].x(), quad[i].y(),
                                        (xFactor * oldGeometry.width() + oldGeometry.x())/qreal(previous->size().width()),
                                        (yFactor * oldGeometry.height() + oldGeometry.y())/qreal(previous->size().height()));
                    newQuad[i] = vertex;
                }
                quads[PreviousContentLeaf].append(newQuad);
            }
        }
    }

    const bool indexedQuads = GLVertexBuffer::supportsIndexedQuads();
    const GLenum primitiveType = indexedQuads ? GL_QUADS : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;

    const size_t size = verticesPerQuad *
        (quads[0].count() + quads[1].count() + quads[2].count() + quads[3].count()) * sizeof(GLVertex2D);

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    GLVertex2D *map = (GLVertex2D *) vbo->map(size);

    LeafNode nodes[LeafCount];
    setupLeafNodes(nodes, quads, data);

    for (int i = 0, v = 0; i < LeafCount; i++) {
        if (quads[i].isEmpty() || !nodes[i].texture)
            continue;

        nodes[i].firstVertex = v;
        nodes[i].vertexCount = quads[i].count() * verticesPerQuad;

        const QMatrix4x4 matrix = nodes[i].texture->matrix(nodes[i].coordinateType);

        quads[i].makeInterleavedArrays(primitiveType, &map[v], matrix);
        v += quads[i].count() * verticesPerQuad;
    }

    vbo->unmap();
    vbo->bindArrays();

    // Make sure the blend function is set up correctly in case we will be doing blending
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    float opacity = -1.0;

    for (int i = 0; i < LeafCount; i++) {
        if (nodes[i].vertexCount == 0)
            continue;

        setBlendEnabled(nodes[i].hasAlpha || nodes[i].opacity < 1.0);

        if (opacity != nodes[i].opacity) {
            shader->setUniform(GLShader::ModulationConstant,
                               modulate(nodes[i].opacity, data.brightness()));
            opacity = nodes[i].opacity;
        }

        nodes[i].texture->setFilter(filter);
        nodes[i].texture->setWrapMode(GL_CLAMP_TO_EDGE);
        nodes[i].texture->bind();

        vbo->draw(region, primitiveType, nodes[i].firstVertex, nodes[i].vertexCount, m_hardwareClipping);
    }

    vbo->unbindArrays();

    setBlendEnabled(false);

    if (!data.shader)
        ShaderManager::instance()->popShader();

    endRenderWindow();
}


//****************************************
// OpenGLWindowPixmap
//****************************************

OpenGLWindowPixmap::OpenGLWindowPixmap(Scene::Window *window, SceneOpenGL* scene)
    : WindowPixmap(window)
    , m_texture(scene->createTexture())
{
}

OpenGLWindowPixmap::~OpenGLWindowPixmap()
{
}

bool OpenGLWindowPixmap::bind()
{
    if (!m_texture->isNull()) {
        if (!toplevel()->damage().isEmpty()) {
            // mipmaps need to be updated
            m_texture->setDirty();
            toplevel()->resetDamage();
        }
        return true;
    }
    if (!isValid()) {
        return false;
    }

    bool success = m_texture->load(pixmap(), toplevel()->size(), toplevel()->visual());

    if (success)
        toplevel()->resetDamage();
    else
        qDebug() << "Failed to bind window";
    return success;
}

//****************************************
// SceneOpenGL::EffectFrame
//****************************************

GLTexture* SceneOpenGL::EffectFrame::m_unstyledTexture = NULL;
QPixmap* SceneOpenGL::EffectFrame::m_unstyledPixmap = NULL;

SceneOpenGL::EffectFrame::EffectFrame(EffectFrameImpl* frame, SceneOpenGL *scene)
    : Scene::EffectFrame(frame)
    , m_texture(NULL)
    , m_textTexture(NULL)
    , m_oldTextTexture(NULL)
    , m_textPixmap(NULL)
    , m_iconTexture(NULL)
    , m_oldIconTexture(NULL)
    , m_selectionTexture(NULL)
    , m_unstyledVBO(NULL)
    , m_scene(scene)
{
    if (m_effectFrame->style() == EffectFrameUnstyled && !m_unstyledTexture) {
        updateUnstyledTexture();
    }
}

SceneOpenGL::EffectFrame::~EffectFrame()
{
    delete m_texture;
    delete m_textTexture;
    delete m_textPixmap;
    delete m_oldTextTexture;
    delete m_iconTexture;
    delete m_oldIconTexture;
    delete m_selectionTexture;
    delete m_unstyledVBO;
}

void SceneOpenGL::EffectFrame::free()
{
    glFlush();
    delete m_texture;
    m_texture = NULL;
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
    delete m_iconTexture;
    m_iconTexture = NULL;
    delete m_selectionTexture;
    m_selectionTexture = NULL;
    delete m_unstyledVBO;
    m_unstyledVBO = NULL;
    delete m_oldIconTexture;
    m_oldIconTexture = NULL;
    delete m_oldTextTexture;
    m_oldTextTexture = NULL;
}

void SceneOpenGL::EffectFrame::freeIconFrame()
{
    delete m_iconTexture;
    m_iconTexture = NULL;
}

void SceneOpenGL::EffectFrame::freeTextFrame()
{
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
}

void SceneOpenGL::EffectFrame::freeSelection()
{
    delete m_selectionTexture;
    m_selectionTexture = NULL;
}

void SceneOpenGL::EffectFrame::crossFadeIcon()
{
    delete m_oldIconTexture;
    m_oldIconTexture = m_iconTexture;
    m_iconTexture = NULL;
}

void SceneOpenGL::EffectFrame::crossFadeText()
{
    delete m_oldTextTexture;
    m_oldTextTexture = m_textTexture;
    m_textTexture = NULL;
}

void SceneOpenGL::EffectFrame::render(QRegion region, double opacity, double frameOpacity)
{
    if (m_effectFrame->geometry().isEmpty())
        return; // Nothing to display

    region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    GLShader* shader = m_effectFrame->shader();
    bool sceneShader = false;
    if (!shader) {
        shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
        sceneShader = true;
    } else if (shader) {
        ShaderManager::instance()->pushShader(shader);
    }

    if (shader) {
        if (sceneShader)
            shader->setUniform(GLShader::Offset, QVector2D(0, 0));

        shader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));
        shader->setUniform(GLShader::Saturation, 1.0f);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the actual frame
    if (m_effectFrame->style() == EffectFrameUnstyled) {
        if (!m_unstyledVBO) {
            m_unstyledVBO = new GLVertexBuffer(GLVertexBuffer::Static);
            QRect area = m_effectFrame->geometry();
            area.moveTo(0, 0);
            area.adjust(-5, -5, 5, 5);

            const int roundness = 5;
            QVector<float> verts, texCoords;
            verts.reserve(84);
            texCoords.reserve(84);

            // top left
            verts << area.left() << area.top();
            texCoords << 0.0f << 0.0f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top right
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            // bottom left
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom right
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom();
            texCoords << 1.0f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            // center
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;

            m_unstyledVBO->setData(verts.count() / 2, 2, verts.data(), texCoords.data());
        }

        if (shader) {
            const float a = opacity * frameOpacity;
            shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
        }

        m_unstyledTexture->bind();
        const QPoint pt = m_effectFrame->geometry().topLeft();
        if (sceneShader) {
            shader->setUniform(GLShader::Offset, QVector2D(pt.x(), pt.y()));
        } else {
            QMatrix4x4 translation;
            translation.translate(pt.x(), pt.y());
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, translation);
            }
        }
        m_unstyledVBO->render(region, GL_TRIANGLES);
        if (!sceneShader) {
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
            }
        }
        m_unstyledTexture->unbind();
    } else if (m_effectFrame->style() == EffectFrameStyled) {
        if (!m_texture)   // Lazy creation
            updateTexture();

        if (shader) {
            const float a = opacity * frameOpacity;
            shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
        }
        m_texture->bind();
        qreal left, top, right, bottom;
        m_effectFrame->frame().getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        m_texture->render(region, m_effectFrame->geometry().adjusted(-left, -top, right, bottom));
        m_texture->unbind();

    }
    if (!m_effectFrame->selection().isNull()) {
        if (!m_selectionTexture) { // Lazy creation
            QPixmap pixmap = m_effectFrame->selectionFrame().framePixmap();
            if (!pixmap.isNull())
                m_selectionTexture = new GLTexture(pixmap);
        }
        if (m_selectionTexture) {
            if (shader) {
                const float a = opacity * frameOpacity;
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            m_selectionTexture->bind();
            m_selectionTexture->render(region, m_effectFrame->selection());
            m_selectionTexture->unbind();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // Render icon
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
        QPoint topLeft(m_effectFrame->geometry().x(),
                       m_effectFrame->geometry().center().y() - m_effectFrame->iconSize().height() / 2);

        if (m_effectFrame->isCrossFade() && m_oldIconTexture) {
            if (shader) {
                const float a = opacity * (1.0 - m_effectFrame->crossFadeProgress());
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }

            m_oldIconTexture->bind();
            m_oldIconTexture->render(region, QRect(topLeft, m_effectFrame->iconSize()));
            m_oldIconTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
        }

        if (!m_iconTexture) { // lazy creation
            m_iconTexture = new GLTexture(m_effectFrame->icon().pixmap(m_effectFrame->iconSize()));
        }
        m_iconTexture->bind();
        m_iconTexture->render(region, QRect(topLeft, m_effectFrame->iconSize()));
        m_iconTexture->unbind();
    }

    // Render text
    if (!m_effectFrame->text().isEmpty()) {
        if (m_effectFrame->isCrossFade() && m_oldTextTexture) {
            if (shader) {
                const float a = opacity * (1.0 - m_effectFrame->crossFadeProgress());
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }

            m_oldTextTexture->bind();
            m_oldTextTexture->render(region, m_effectFrame->geometry());
            m_oldTextTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
        }
        if (!m_textTexture)   // Lazy creation
            updateTextTexture();
        m_textTexture->bind();
        m_textTexture->render(region, m_effectFrame->geometry());
        m_textTexture->unbind();
    }

    if (shader) {
        ShaderManager::instance()->popShader();
    }
    glDisable(GL_BLEND);
}

void SceneOpenGL::EffectFrame::updateTexture()
{
    delete m_texture;
    m_texture = 0L;
    if (m_effectFrame->style() == EffectFrameStyled) {
        QPixmap pixmap = m_effectFrame->frame().framePixmap();
        m_texture = new GLTexture(pixmap);
    }
}

void SceneOpenGL::EffectFrame::updateTextTexture()
{
    delete m_textTexture;
    m_textTexture = 0L;
    delete m_textPixmap;
    m_textPixmap = 0L;

    if (m_effectFrame->text().isEmpty())
        return;

    // Determine position on texture to paint text
    QRect rect(QPoint(0, 0), m_effectFrame->geometry().size());
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty())
        rect.setLeft(m_effectFrame->iconSize().width());

    // If static size elide text as required
    QString text = m_effectFrame->text();
    if (m_effectFrame->isStatic()) {
        QFontMetrics metrics(m_effectFrame->font());
        text = metrics.elidedText(text, Qt::ElideRight, rect.width());
    }

    m_textPixmap = new QPixmap(m_effectFrame->geometry().size());
    m_textPixmap->fill(Qt::transparent);
    QPainter p(m_textPixmap);
    p.setFont(m_effectFrame->font());
    if (m_effectFrame->style() == EffectFrameStyled)
        p.setPen(m_effectFrame->styledTextColor());
    else // TODO: What about no frame? Custom color setting required
        p.setPen(Qt::white);
    p.drawText(rect, m_effectFrame->alignment(), text);
    p.end();
    m_textTexture = new GLTexture(*m_textPixmap);
}

void SceneOpenGL::EffectFrame::updateUnstyledTexture()
{
    delete m_unstyledTexture;
    m_unstyledTexture = 0L;
    delete m_unstyledPixmap;
    m_unstyledPixmap = 0L;
    // Based off circle() from kwinxrenderutils.cpp
#define CS 8
    m_unstyledPixmap = new QPixmap(2 * CS, 2 * CS);
    m_unstyledPixmap->fill(Qt::transparent);
    QPainter p(m_unstyledPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawEllipse(m_unstyledPixmap->rect());
    p.end();
#undef CS
    m_unstyledTexture = new GLTexture(*m_unstyledPixmap);
}

void SceneOpenGL::EffectFrame::cleanup()
{
    delete m_unstyledTexture;
    m_unstyledTexture = NULL;
    delete m_unstyledPixmap;
    m_unstyledPixmap = NULL;
}

//****************************************
// SceneOpenGL::Shadow
//****************************************
class DecorationShadowTextureCache
{
public:
    ~DecorationShadowTextureCache();
    DecorationShadowTextureCache(const DecorationShadowTextureCache&) = delete;
    static DecorationShadowTextureCache &instance();

    void unregister(SceneOpenGLShadow *shadow);
    QSharedPointer<GLTexture> getTexture(SceneOpenGLShadow *shadow);

private:
    DecorationShadowTextureCache() = default;
    struct Data {
        QSharedPointer<GLTexture> texture;
        QVector<SceneOpenGLShadow*> shadows;
    };
    QHash<KDecoration2::DecorationShadow*, Data> m_cache;
};

DecorationShadowTextureCache &DecorationShadowTextureCache::instance()
{
    static DecorationShadowTextureCache s_instance;
    return s_instance;
}

DecorationShadowTextureCache::~DecorationShadowTextureCache()
{
    Q_ASSERT(m_cache.isEmpty());
}

void DecorationShadowTextureCache::unregister(SceneOpenGLShadow *shadow)
{
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        auto &d = it.value();
        // check whether the Vector of Shadows contains our shadow and remove all of them
        auto glIt = d.shadows.begin();
        while (glIt != d.shadows.end()) {
            if (*glIt == shadow) {
                glIt = d.shadows.erase(glIt);
            } else {
                glIt++;
            }
        }
        // if there are no shadows any more we can erase the cache entry
        if (d.shadows.isEmpty()) {
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
}

QSharedPointer<GLTexture> DecorationShadowTextureCache::getTexture(SceneOpenGLShadow *shadow)
{
    Q_ASSERT(shadow->hasDecorationShadow());
    unregister(shadow);
    const auto &decoShadow = shadow->decorationShadow();
    Q_ASSERT(!decoShadow.isNull());
    auto it = m_cache.find(decoShadow.data());
    if (it != m_cache.end()) {
        Q_ASSERT(!it.value().shadows.contains(shadow));
        it.value().shadows << shadow;
        return it.value().texture;
    }
    Data d;
    d.shadows << shadow;
    d.texture = QSharedPointer<GLTexture>::create(shadow->decorationShadowImage());
    m_cache.insert(decoShadow.data(), d);
    return d.texture;
}

SceneOpenGLShadow::SceneOpenGLShadow(Toplevel *toplevel)
    : Shadow(toplevel)
{
}

SceneOpenGLShadow::~SceneOpenGLShadow()
{
    effects->makeOpenGLContextCurrent();
    DecorationShadowTextureCache::instance().unregister(this);
    m_texture.reset();
}

void SceneOpenGLShadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QSizeF top(elementSize(ShadowElementTop));
    const QSizeF topRight(elementSize(ShadowElementTopRight));
    const QSizeF right(elementSize(ShadowElementRight));
    const QSizeF bottomRight(elementSize(ShadowElementBottomRight));
    const QSizeF bottom(elementSize(ShadowElementBottom));
    const QSizeF bottomLeft(elementSize(ShadowElementBottomLeft));
    const QSizeF left(elementSize(ShadowElementLeft));
    const QSizeF topLeft(elementSize(ShadowElementTopLeft));
    if ((left.width() - leftOffset() > topLevel()->width()) ||
        (right.width() - rightOffset() > topLevel()->width()) ||
        (top.height() - topOffset() > topLevel()->height()) ||
        (bottom.height() - bottomOffset() > topLevel()->height())) {
        // if our shadow is bigger than the window, we don't render the shadow
        setShadowRegion(QRegion());
        return;
    }

    const QRectF outerRect(QPointF(-leftOffset(), -topOffset()),
                           QPointF(topLevel()->width() + rightOffset(), topLevel()->height() + bottomOffset()));

    const qreal width = topLeft.width() + top.width() + topRight.width();
    const qreal height = topLeft.height() + left.height() + bottomLeft.height();

    qreal tx1(0.0), tx2(0.0), ty1(0.0), ty2(0.0);

    tx2 = topLeft.width()/width;
    ty2 = topLeft.height()/height;
    WindowQuad topLeftQuad(WindowQuadShadow);
    topLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                      outerRect.y(), tx1, ty1);
    topLeftQuad[ 1 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y(), tx2, ty1);
    topLeftQuad[ 2 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y() + topLeft.height(), tx2, ty2);
    topLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                      outerRect.y() + topLeft.height(), tx1, ty2);
    m_shadowQuads.append(topLeftQuad);

    tx1 = tx2;
    tx2 = (topLeft.width() + top.width())/width;
    ty2 = top.height()/height;
    WindowQuad topQuad(WindowQuadShadow);
    topQuad[ 0 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y(), tx1, ty1);
    topQuad[ 1 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y(), tx2, ty1);
    topQuad[ 2 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y() + top.height(),tx2, ty2);
    topQuad[ 3 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y() + top.height(), tx1, ty2);
    m_shadowQuads.append(topQuad);

    tx1 = tx2;
    tx2 = 1.0;
    ty2 = topRight.height()/height;
    WindowQuad topRightQuad(WindowQuadShadow);
    topRightQuad[ 0 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y(), tx1, ty1);
    topRightQuad[ 1 ] = WindowVertex(outerRect.right(),                     outerRect.y(), tx2, ty1);
    topRightQuad[ 2 ] = WindowVertex(outerRect.right(),                     outerRect.y() + topRight.height(), tx2, ty2);
    topRightQuad[ 3 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y() + topRight.height(), tx1, ty2);
    m_shadowQuads.append(topRightQuad);

    tx1 = (width - right.width())/width;
    ty1 = topRight.height()/height;
    ty2 = (topRight.height() + right.height())/height;
    WindowQuad rightQuad(WindowQuadShadow);
    rightQuad[ 0 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.y() + topRight.height(), tx1, ty1);
    rightQuad[ 1 ] = WindowVertex(outerRect.right(),                    outerRect.y() + topRight.height(), tx2, ty1);
    rightQuad[ 2 ] = WindowVertex(outerRect.right(),                    outerRect.bottom() - bottomRight.height(), tx2, ty2);
    rightQuad[ 3 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty2);
    m_shadowQuads.append(rightQuad);

    tx1 = (width - bottomRight.width())/width;
    ty1 = ty2;
    ty2 = 1.0;
    WindowQuad bottomRightQuad(WindowQuadShadow);
    bottomRightQuad[ 0 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty1);
    bottomRightQuad[ 1 ] = WindowVertex(outerRect.right(),                          outerRect.bottom() - bottomRight.height(), tx2, ty1);
    bottomRightQuad[ 2 ] = WindowVertex(outerRect.right(),                          outerRect.bottom(), tx2, ty2);
    bottomRightQuad[ 3 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomRightQuad);

    tx2 = tx1;
    tx1 = bottomLeft.width()/width;
    ty1 = (height - bottom.height())/height;
    WindowQuad bottomQuad(WindowQuadShadow);
    bottomQuad[ 0 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom() - bottom.height(), tx1, ty1);
    bottomQuad[ 1 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom() - bottom.height(), tx2, ty1);
    bottomQuad[ 2 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom(), tx2, ty2);
    bottomQuad[ 3 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomQuad);

    tx1 = 0.0;
    tx2 = bottomLeft.width()/width;
    ty1 = (height - bottomLeft.height())/height;
    WindowQuad bottomLeftQuad(WindowQuadShadow);
    bottomLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                       outerRect.bottom() - bottomLeft.height(), tx1, ty1);
    bottomLeftQuad[ 1 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty1);
    bottomLeftQuad[ 2 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom(), tx2, ty2);
    bottomLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                       outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomLeftQuad);

    tx2 = left.width()/width;
    ty2 = ty1;
    ty1 = topLeft.height()/height;
    WindowQuad leftQuad(WindowQuadShadow);
    leftQuad[ 0 ] = WindowVertex(outerRect.x(),                 outerRect.y() + topLeft.height(), tx1, ty1);
    leftQuad[ 1 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.y() + topLeft.height(), tx2, ty1);
    leftQuad[ 2 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty2);
    leftQuad[ 3 ] = WindowVertex(outerRect.x(),                 outerRect.bottom() - bottomLeft.height(), tx1, ty2);
    m_shadowQuads.append(leftQuad);
}

bool SceneOpenGLShadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        // simplifies a lot by going directly to
        effects->makeOpenGLContextCurrent();
        m_texture = DecorationShadowTextureCache::instance().getTexture(this);

        return true;
    }
    const QSize top(shadowPixmap(ShadowElementTop).size());
    const QSize topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSize right(shadowPixmap(ShadowElementRight).size());
    const QSize bottom(shadowPixmap(ShadowElementBottom).size());
    const QSize bottomLeft(shadowPixmap(ShadowElementBottomLeft).size());
    const QSize left(shadowPixmap(ShadowElementLeft).size());
    const QSize topLeft(shadowPixmap(ShadowElementTopLeft).size());

    const int width = topLeft.width() + top.width() + topRight.width();
    const int height = topLeft.height() + left.height() + bottomLeft.height();

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    p.drawPixmap(0, 0, shadowPixmap(ShadowElementTopLeft));
    p.drawPixmap(topLeft.width(), 0, shadowPixmap(ShadowElementTop));
    p.drawPixmap(topLeft.width() + top.width(), 0, shadowPixmap(ShadowElementTopRight));
    p.drawPixmap(0, topLeft.height(), shadowPixmap(ShadowElementLeft));
    p.drawPixmap(width - right.width(), topRight.height(), shadowPixmap(ShadowElementRight));
    p.drawPixmap(0, topLeft.height() + left.height(), shadowPixmap(ShadowElementBottomLeft));
    p.drawPixmap(bottomLeft.width(), height - bottom.height(), shadowPixmap(ShadowElementBottom));
    p.drawPixmap(bottomLeft.width() + bottom.width(), topRight.height() + right.height(), shadowPixmap(ShadowElementBottomRight));
    p.end();

    effects->makeOpenGLContextCurrent();
    m_texture = QSharedPointer<GLTexture>::create(image);

    return true;
}

SwapProfiler::SwapProfiler()
{
    init();
}

void SwapProfiler::init()
{
    m_time = 2 * 1000*1000; // we start with a long time mean of 2ms ...
    m_counter = 0;
}

void SwapProfiler::begin()
{
    m_timer.start();
}

char SwapProfiler::end()
{
    // .. and blend in actual values.
    // this way we prevent extremes from killing our long time mean
    m_time = (10*m_time + m_timer.nsecsElapsed())/11;
    if (++m_counter > 500) {
        const bool blocks = m_time > 1000 * 1000; // 1ms, i get ~250µs and ~7ms w/o triple buffering...
        qDebug() << "Triple buffering detection:" << QString(blocks ? QStringLiteral("NOT available") : QStringLiteral("Available")) <<
                        " - Mean block time:" << m_time/(1000.0*1000.0) << "ms";
        return blocks ? 'd' : 't';
    }
    return 0;
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : Renderer(client)
    , m_texture()
{
    connect(this, &Renderer::renderScheduled, client->client(), static_cast<void (Client::*)(const QRect&)>(&Client::addRepaint));
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer() = default;

// Rotates the given source rect 90° counter-clockwise,
// and flips it vertically
static QImage rotate(const QImage &srcImage, const QRect &srcRect)
{
    QImage image(srcRect.height(), srcRect.width(), srcImage.format());

    const uint32_t *src = reinterpret_cast<const uint32_t *>(srcImage.bits());
    uint32_t *dst = reinterpret_cast<uint32_t *>(image.bits());

    for (int x = 0; x < image.width(); x++) {
        const uint32_t *s = src + (srcRect.y() + x) * srcImage.width() + srcRect.x();
        uint32_t *d = dst + x;

        for (int y = 0; y < image.height(); y++) {
            *d = s[y];
            d += image.width();
        }
    }

    return image;
}

void SceneOpenGLDecorationRenderer::render()
{
    const QRegion scheduled = getScheduled();
    if (scheduled.isEmpty()) {
        return;
    }
    if (areImageSizesDirty()) {
        resizeTexture();
        resetImageSizesDirty();
    }

    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    const QRect geometry = scheduled.boundingRect();

    auto renderPart = [this](const QRect &geo, const QRect &partRect, const QPoint &offset, bool rotated = false) {
        if (geo.isNull()) {
            return;
        }
        QImage image = renderToImage(geo);
        if (rotated) {
            // TODO: get this done directly when rendering to the image
            image = rotate(image, QRect(geo.topLeft() - partRect.topLeft(), geo.size()));
        }
        m_texture->update(image, geo.topLeft() - partRect.topLeft() + offset);
    };
    renderPart(left.intersected(geometry), left, QPoint(0, top.height() + bottom.height() + 2), true);
    renderPart(top.intersected(geometry), top, QPoint(0, 0));
    renderPart(right.intersected(geometry), right, QPoint(0, top.height() + bottom.height() + left.width() + 3), true);
    renderPart(bottom.intersected(geometry), bottom, QPoint(0, top.height() + 1));
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

void SceneOpenGLDecorationRenderer::resizeTexture()
{
    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);
    QSize size;

    size.rwidth() = qMax(qMax(top.width(), bottom.width()),
                         qMax(left.height(), right.height()));
    size.rheight() = top.height() + bottom.height() +
                     left.width() + right.width() + 3;

    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size)
        return;

    if (!size.isEmpty()) {
        m_texture.reset(new GLTexture(size.width(), size.height()));
        m_texture->setYInverted(true);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_texture->clear();
    } else {
        m_texture.reset();
    }
}

void SceneOpenGLDecorationRenderer::reparent(Deleted *deleted)
{
    render();
    Renderer::reparent(deleted);
}

} // namespace
