/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    Explicit command stream synchronization based on the sample
    implementation by James Jones <jajones@nvidia.com>,

    SPDX-FileCopyrightText: 2011 NVIDIA Corporation

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_opengl.h"

#include "platform.h"
#include "wayland_server.h"
#include "platformsupport/scenes/opengl/texture.h"

#include <kwinglplatform.h>
#include <kwineffectquickview.h>

#include "utils.h"
#include "x11client.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "main.h"
#include "overlaywindow.h"
#include "screens.h"
#include "cursor.h"
#include "decorations/decoratedclient.h"
#include <logging.h>

#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/surface_interface.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <unistd.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QGraphicsScale>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

#include <KLocalizedString>
#include <KNotification>
#include <KProcess>

// HACK: workaround for libepoxy < 1.3
#ifndef GL_GUILTY_CONTEXT_RESET
#define GL_GUILTY_CONTEXT_RESET 0x8253
#endif
#ifndef GL_INNOCENT_CONTEXT_RESET
#define GL_INNOCENT_CONTEXT_RESET 0x8254
#endif
#ifndef GL_UNKNOWN_CONTEXT_RESET
#define GL_UNKNOWN_CONTEXT_RESET 0x8255
#endif

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
    // If glDeleteSync is called before the xcb fence is signalled
    // the nvidia driver (the only one to implement GL_SYNC_X11_FENCE_EXT)
    // deadlocks waiting for the fence to be signalled.
    // To avoid this, make sure the fence is signalled before
    // deleting the sync.
    if (m_state == Resetting || m_state == Ready){
        trigger();
        // The flush is necessary!
        // The trigger command needs to be sent to the X server.
        xcb_flush(connection());
    }
    xcb_sync_destroy_fence(connection(), m_fence);
    glDeleteSync(m_sync);

    if (m_state == Resetting)
        xcb_discard_reply(connection(), m_reset_cookie.sequence);
}

void SyncObject::trigger()
{
    Q_ASSERT(m_state == Ready || m_state == Resetting);

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
    Q_ASSERT(m_state == TriggerSent || m_state == Waiting);

    // Check if the fence is signaled
    GLint value;
    glGetSynciv(m_sync, GL_SYNC_STATUS, 1, nullptr, &value);

    if (value != GL_SIGNALED) {
        qCDebug(KWIN_OPENGL) << "Waiting for X fence to finish";

        // Wait for the fence to become signaled with a one second timeout
        const GLenum result = glClientWaitSync(m_sync, 0, 1000000000);

        switch (result) {
        case GL_TIMEOUT_EXPIRED:
            qCWarning(KWIN_OPENGL) << "Timeout while waiting for X fence";
            return false;

        case GL_WAIT_FAILED:
            qCWarning(KWIN_OPENGL) << "glClientWaitSync() failed";
            return false;
        }
    }

    m_state = Done;
    return true;
}

void SyncObject::reset()
{
    Q_ASSERT(m_state == Done);

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
    Q_ASSERT(m_state == Resetting);
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

/************************************************
 * SceneOpenGL
 ***********************************************/

SceneOpenGL::SceneOpenGL(OpenGLBackend *backend, QObject *parent)
    : Scene(parent)
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
    if (!glPlatform->isGLES() && !hasGLExtension(QByteArrayLiteral("GL_ARB_texture_non_power_of_two"))
            && !hasGLExtension(QByteArrayLiteral("GL_ARB_texture_rectangle"))) {
        qCCritical(KWIN_OPENGL) << "GL_ARB_texture_non_power_of_two and GL_ARB_texture_rectangle missing";
        init_ok = false;
        return; // error
    }
    if (glPlatform->isMesaDriver() && glPlatform->mesaVersion() < kVersionNumber(10, 0)) {
        qCCritical(KWIN_OPENGL) << "KWin requires at least Mesa 10.0 for OpenGL compositing.";
        init_ok = false;
        return;
    }

    m_debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;
    initDebugOutput();

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!glPlatform->supports(LooseBinding));
    }

    bool haveSyncObjects = glPlatform->isGLES()
        ? hasGLVersion(3, 0)
        : hasGLVersion(3, 2) || hasGLExtension("GL_ARB_sync");

    if (hasGLExtension("GL_EXT_x11_sync_object") && haveSyncObjects && kwinApp()->operationMode() == Application::OperationModeX11) {
        const QByteArray useExplicitSync = qgetenv("KWIN_EXPLICIT_SYNC");

        if (useExplicitSync != "0") {
            qCDebug(KWIN_OPENGL) << "Initializing fences for synchronization with the X command stream";
            m_syncManager = new SyncManager;
        } else {
            qCDebug(KWIN_OPENGL) << "Explicit synchronization with the X command stream disabled by environment variable";
        }
    }
}

SceneOpenGL::~SceneOpenGL()
{
    if (init_ok) {
        makeOpenGLContextCurrent();
    }
    SceneOpenGL::EffectFrame::cleanup();

    delete m_syncManager;

    // backend might be still needed for a different scene
    delete m_backend;
}


void SceneOpenGL::initDebugOutput()
{
    const bool have_KHR_debug = hasGLExtension(QByteArrayLiteral("GL_KHR_debug"));
    const bool have_ARB_debug = hasGLExtension(QByteArrayLiteral("GL_ARB_debug_output"));
    if (!have_KHR_debug && !have_ARB_debug)
        return;

    if (!have_ARB_debug) {
        // if we don't have ARB debug, but only KHR debug we need to verify whether the context is a debug context
        // it should work without as well, but empirical tests show: no it doesn't
        if (GLPlatform::instance()->isGLES()) {
            if (!hasGLVersion(3, 2)) {
                // empirical data shows extension doesn't work
                return;
            }
        } else if (!hasGLVersion(3, 0)) {
            return;
        }
        // can only be queried with either OpenGL >= 3.0 or OpenGL ES of at least 3.1
        GLint value = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &value);
        if (!(value & GL_CONTEXT_FLAG_DEBUG_BIT)) {
            return;
        }
    }

    // Set the callback function
    auto callback = [](GLenum source, GLenum type, GLuint id,
                       GLenum severity, GLsizei length,
                       const GLchar *message,
                       const GLvoid *userParam) {
        Q_UNUSED(source)
        Q_UNUSED(severity)
        Q_UNUSED(userParam)
        while (length && std::isspace(message[length - 1])) {
            --length;
        }

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            qCWarning(KWIN_OPENGL, "%#x: %.*s", id, length, message);
            break;

        case GL_DEBUG_TYPE_OTHER:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        default:
            qCDebug(KWIN_OPENGL, "%#x: %.*s", id, length, message);
            break;
        }
    };

    glDebugMessageCallback(callback, nullptr);

    // This state exists only in GL_KHR_debug
    if (have_KHR_debug)
        glEnable(GL_DEBUG_OUTPUT);

#if !defined(QT_NO_DEBUG)
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

SceneOpenGL *SceneOpenGL::createScene(QObject *parent)
{
    OpenGLBackend *backend = kwinApp()->platform()->createOpenGLBackend();
    if (!backend) {
        return nullptr;
    }
    if (!backend->isFailed()) {
        backend->init();
    }
    if (backend->isFailed()) {
        delete backend;
        return nullptr;
    }
    SceneOpenGL *scene = nullptr;
    // first let's try an OpenGL 2 scene
    if (SceneOpenGL2::supported(backend)) {
        scene = new SceneOpenGL2(backend, parent);
        if (scene->initFailed()) {
            delete scene;
            scene = nullptr;
        } else {
            return scene;
        }
    }
    if (!scene) {
        if (GLPlatform::instance()->recommendedCompositor() == XRenderCompositing) {
            qCCritical(KWIN_OPENGL) << "OpenGL driver recommends XRender based compositing. Falling back to XRender.";
            qCCritical(KWIN_OPENGL) << "To overwrite the detection use the environment variable KWIN_COMPOSE";
            qCCritical(KWIN_OPENGL) << "For more information see https://community.kde.org/KWin/Environment_Variables#KWIN_COMPOSE";
        }
        delete backend;
    }

    return scene;
}

OverlayWindow *SceneOpenGL::overlayWindow() const
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

void SceneOpenGL::handleGraphicsReset(GLenum status)
{
    switch (status) {
    case GL_GUILTY_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset attributable to the current GL context occurred.";
        break;

    case GL_INNOCENT_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset not attributable to the current GL context occurred.";
        break;

    case GL_UNKNOWN_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset of an unknown cause occurred.";
        break;

    default:
        break;
    }

    QElapsedTimer timer;
    timer.start();

    // Wait until the reset is completed or max 10 seconds
    while (timer.elapsed() < 10000 && glGetGraphicsResetStatus() != GL_NO_ERROR)
        usleep(50);

    qCDebug(KWIN_OPENGL) << "Attempting to reset compositing.";
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

/**
 * Render cursor texture in case hardware cursor is disabled.
 * Useful for screen recording apps or backends that can't do planes.
 */
void SceneOpenGL2::paintCursor(const QRegion &rendered)
{
    Cursor* cursor = Cursors::self()->currentCursor();

    // don't paint if we use hardware cursor or the cursor is hidden
    if (!kwinApp()->platform()->usesSoftwareCursor() ||
        kwinApp()->platform()->isCursorHidden() ||
        cursor->image().isNull()) {
        return;
    }

    // figure out which part of the cursor needs to be repainted
    const QPoint cursorPos = cursor->pos() - cursor->hotspot();
    const QRect cursorRect = cursor->rect();
    QRegion region;
    for (const QRect &rect : rendered) {
        region |= rect.translated(-cursorPos).intersected(cursorRect);
    }
    if (region.isEmpty()) {
        return;
    }

    // lazy init texture cursor only in case we need software rendering
    if (!m_cursorTexture) {
        auto updateCursorTexture = [this] {
            // don't paint if no image for cursor is set
            const QImage img = Cursors::self()->currentCursor()->image();
            if (img.isNull()) {
                return;
            }
            m_cursorTexture.reset(new GLTexture(img));
        };

        // init now
        updateCursorTexture();

        // handle shape update on case cursor image changed
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, updateCursorTexture);
    }

    // get cursor position in projection coordinates
    QMatrix4x4 mvp = m_projectionMatrix;
    mvp.translate(cursorPos.x(), cursorPos.y());

    // handle transparence
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // paint texture in cursor offset
    m_cursorTexture->bind();
    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_cursorTexture->render(region, cursorRect);
    m_cursorTexture->unbind();
    glDisable(GL_BLEND);
}

void SceneOpenGL::aboutToStartPainting(const QRegion &damage)
{
    m_backend->aboutToStartPainting(damage);
}

qint64 SceneOpenGL::paint(const QRegion &damage, const QList<Toplevel *> &toplevels)
{
    // actually paint the frame, flushed with the NEXT frame
    createStackingOrder(toplevels);

    // After this call, updateRegion will contain the damaged region in the
    // back buffer. This is the region that needs to be posted to repair
    // the front buffer. It doesn't include the additional damage returned
    // by prepareRenderingFrame(). validRegion is the region that has been
    // repainted, and may be larger than updateRegion.
    QRegion updateRegion, validRegion;
    if (m_backend->perScreenRendering()) {
        // trigger start render timer
        m_backend->prepareRenderingFrame();
        for (int i = 0; i < screens()->count(); ++i) {
            const QRect &geo = screens()->geometry(i);
            const qreal scaling = screens()->scale(i);
            QRegion update;
            QRegion valid;
            // prepare rendering makes context current on the output
            QRegion repaint = m_backend->prepareRenderingForScreen(i);
            GLVertexBuffer::setVirtualScreenGeometry(geo);
            GLRenderTarget::setVirtualScreenGeometry(geo);
            GLVertexBuffer::setVirtualScreenScale(scaling);
            GLRenderTarget::setVirtualScreenScale(scaling);

            const GLenum status = glGetGraphicsResetStatus();
            if (status != GL_NO_ERROR) {
                handleGraphicsReset(status);
                return 0;
            }

            int mask = 0;
            updateProjectionMatrix();

            paintScreen(&mask, damage.intersected(geo), repaint, &update, &valid, projectionMatrix(), geo, scaling);   // call generic implementation
            paintCursor(valid);

            GLVertexBuffer::streamingBuffer()->endOfFrame();

            m_backend->endRenderingFrameForScreen(i, valid, update);

            GLVertexBuffer::streamingBuffer()->framePosted();
        }
    } else {
        m_backend->makeCurrent();
        QRegion repaint = m_backend->prepareRenderingFrame();

        const GLenum status = glGetGraphicsResetStatus();
        if (status != GL_NO_ERROR) {
            handleGraphicsReset(status);
            return 0;
        }
        GLVertexBuffer::setVirtualScreenGeometry(screens()->geometry());
        GLRenderTarget::setVirtualScreenGeometry(screens()->geometry());
        GLVertexBuffer::setVirtualScreenScale(1);
        GLRenderTarget::setVirtualScreenScale(1);

        int mask = 0;
        updateProjectionMatrix();
        paintScreen(&mask, damage, repaint, &updateRegion, &validRegion, projectionMatrix());   // call generic implementation

        if (!GLPlatform::instance()->isGLES()) {
            const QSize &screenSize = screens()->size();
            const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());

            // copy dirty parts from front to backbuffer
            if (!m_backend->supportsBufferAge() &&
                options->glPreferBufferSwap() == Options::CopyFrontBuffer &&
                validRegion != displayRegion) {
                glReadBuffer(GL_FRONT);
                m_backend->copyPixels(displayRegion - validRegion);
                glReadBuffer(GL_BACK);
                validRegion = displayRegion;
            }
        }

        GLVertexBuffer::streamingBuffer()->endOfFrame();

        m_backend->endRenderingFrame(validRegion, updateRegion);

        GLVertexBuffer::streamingBuffer()->framePosted();
    }

    if (m_currentFence) {
        if (!m_syncManager->updateFences()) {
            qCDebug(KWIN_OPENGL) << "Aborting explicit synchronization with the X command stream.";
            qCDebug(KWIN_OPENGL) << "Future frames will be rendered unsynchronized.";
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

void SceneOpenGL::paintBackground(const QRegion &region)
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
        for (const QRect &r : region) {
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

SceneOpenGLTexture *SceneOpenGL::createTexture()
{
    return new SceneOpenGLTexture(m_backend);
}

bool SceneOpenGL::viewportLimitsMatched(const QSize &size) const {
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        // TODO: On Wayland we can't suspend. Find a solution that works here as well!
        return true;
    }
    GLint limit[2];
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, limit);
    if (limit[0] < size.width() || limit[1] < size.height()) {
        auto compositor = static_cast<X11Compositor*>(Compositor::self());
        QMetaObject::invokeMethod(compositor, [compositor]() {
            qCDebug(KWIN_OPENGL) << "Suspending compositing because viewport limits are not met";
            compositor->suspend(X11Compositor::AllReasonSuspend);
        }, Qt::QueuedConnection);
        return false;
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
    GLRenderTarget::setVirtualScreenSize(size);
}

void SceneOpenGL::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    const QRect r = region.boundingRect();
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x(), screens()->size().height() - r.y() - r.height(), r.width(), r.height());
    KWin::Scene::paintDesktop(desktop, mask, region, data);
    glDisable(GL_SCISSOR_TEST);
}

void SceneOpenGL::paintEffectQuickView(EffectQuickView *w)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    const QRect rect = w->geometry();

    GLTexture *t = w->bufferAsTexture();
    if (!t) {
        return;
    }

    QMatrix4x4 mvp(projectionMatrix());
    mvp.translate(rect.x(), rect.y());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    t->bind();
    t->render(QRegion(infiniteRegion()), w->geometry());
    t->unbind();
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

bool SceneOpenGL::makeOpenGLContextCurrent()
{
    return m_backend->makeCurrent();
}

void SceneOpenGL::doneOpenGLContextCurrent()
{
    m_backend->doneCurrent();
}

bool SceneOpenGL::supportsSurfacelessContext() const
{
    return m_backend->supportsSurfacelessContext();
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

bool SceneOpenGL::animationsSupported() const
{
    return !GLPlatform::instance()->isSoftwareEmulation();
}

QVector<QByteArray> SceneOpenGL::openGLPlatformInterfaceExtensions() const
{
    return m_backend->extensions().toVector();
}

QSharedPointer<GLTexture> SceneOpenGL::textureForOutput(AbstractOutput* output) const
{
    return m_backend->textureForOutput(output);
}

//****************************************
// SceneOpenGL2
//****************************************
bool SceneOpenGL2::supported(OpenGLBackend *backend)
{
    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O2") == 0 || qstrcmp(forceEnv, "O2ES") == 0) {
            qCDebug(KWIN_OPENGL) << "OpenGL 2 compositing enforced by environment variable";
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
        qCDebug(KWIN_OPENGL) << "Driver does not recommend OpenGL 2 compositing";
        return false;
    }
    return true;
}

SceneOpenGL2::SceneOpenGL2(OpenGLBackend *backend, QObject *parent)
    : SceneOpenGL(backend, parent)
    , m_lanczosFilter(nullptr)
{
    if (!init_ok) {
        // base ctor already failed
        return;
    }

    // We only support the OpenGL 2+ shader API, not GL_ARB_shader_objects
    if (!hasGLVersion(2, 0)) {
        qCDebug(KWIN_OPENGL) << "OpenGL 2.0 is not supported";
        init_ok = false;
        return;
    }

    const QSize &s = screens()->size();
    GLRenderTarget::setVirtualScreenSize(s);
    GLRenderTarget::setVirtualScreenGeometry(screens()->geometry());

    // push one shader on the stack so that one is always bound
    ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    if (checkGLError("Init")) {
        qCCritical(KWIN_OPENGL) << "OpenGL 2 compositing setup failed";
        init_ok = false;
        return; // error
    }

    // It is not legal to not have a vertex array object bound in a core context
    if (!GLPlatform::instance()->isGLES() && hasGLExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }

    if (!ShaderManager::instance()->selfTest()) {
        qCCritical(KWIN_OPENGL) << "ShaderManager self test failed";
        init_ok = false;
        return;
    }

    qCDebug(KWIN_OPENGL) << "OpenGL 2 compositing successfully initialized";
    init_ok = true;
}

SceneOpenGL2::~SceneOpenGL2()
{
    if (m_lanczosFilter) {
        makeOpenGLContextCurrent();
        delete m_lanczosFilter;
        m_lanczosFilter = nullptr;
    }
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

void SceneOpenGL2::updateProjectionMatrix()
{
    m_projectionMatrix = createProjectionMatrix();
}

void SceneOpenGL2::paintSimpleScreen(int mask, const QRegion &region)
{
    m_screenProjectionMatrix = m_projectionMatrix;

    Scene::paintSimpleScreen(mask, region);
}

void SceneOpenGL2::paintGenericScreen(int mask, const ScreenPaintData &data)
{
    const QMatrix4x4 screenMatrix = transformation(mask, data);

    m_screenProjectionMatrix = m_projectionMatrix * screenMatrix;

    Scene::paintGenericScreen(mask, data);
}

void SceneOpenGL2::doPaintBackground(const QVector< float >& vertices)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setData(vertices.count() / 2, 2, vertices.data(), nullptr);

    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, m_projectionMatrix);

    vbo->render(GL_TRIANGLES);
}

Scene::Window *SceneOpenGL2::createWindow(Toplevel *t)
{
    return new OpenGLWindow(t, this);
}

void SceneOpenGL2::finalDrawWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data)
{
    if (waylandServer() && waylandServer()->isScreenLocked() && !w->window()->isLockScreen() && !w->window()->isInputMethod()) {
        return;
    }
    performPaintWindow(w, mask, region, data);
}

void SceneOpenGL2::performPaintWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data)
{
    if (mask & PAINT_WINDOW_LANCZOS) {
        if (!m_lanczosFilter) {
            m_lanczosFilter = new LanczosFilter(this);
            // reset the lanczos filter when the screen gets resized
            // it will get created next paint
            connect(screens(), &Screens::changed, this, [this]() {
                makeOpenGLContextCurrent();
                delete m_lanczosFilter;
                m_lanczosFilter = nullptr;
            });
        }
        m_lanczosFilter->performPaint(w, mask, region, data);
    } else
        w->sceneWindow()->performPaint(mask, region, data);
}

//****************************************
// OpenGLWindow
//****************************************

OpenGLWindow::OpenGLWindow(Toplevel *toplevel, SceneOpenGL *scene)
    : Scene::Window(toplevel)
    , m_scene(scene)
{
}

OpenGLWindow::~OpenGLWindow()
{
}

// Bind the window pixmap to an OpenGL texture.
bool OpenGLWindow::bindTexture()
{
    OpenGLWindowPixmap *pixmap = windowPixmap<OpenGLWindowPixmap>();
    if (!pixmap) {
        return false;
    }
    if (pixmap->isDiscarded()) {
        return !pixmap->texture()->isNull();
    }

    if (!window()->damage().isEmpty())
        m_scene->insertWait();

    return pixmap->bind();
}

QMatrix4x4 OpenGLWindow::transformation(int mask, const WindowPaintData &data) const
{
    QMatrix4x4 matrix;
    matrix.translate(x(), y());

    if (!(mask & Scene::PAINT_WINDOW_TRANSFORMED))
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

bool OpenGLWindow::beginRenderWindow(int mask, const QRegion &region, WindowPaintData &data)
{
    if (region.isEmpty())
        return false;

    m_hardwareClipping = region != infiniteRegion() && (mask & Scene::PAINT_WINDOW_TRANSFORMED) && !(mask & Scene::PAINT_SCREEN_TRANSFORMED);
    if (region != infiniteRegion() && !m_hardwareClipping) {
        WindowQuadList quads;
        quads.reserve(data.quads.count());

        const QRegion filterRegion = region.translated(-x(), -y());
        // split all quads in bounding rect with the actual rects in the region
        for (const WindowQuad &quad : qAsConst(data.quads)) {
            for (const QRect &r : filterRegion) {
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

    if (!bindTexture()) {
        return false;
    }

    if (m_hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    const GLVertexAttrib attribs[] = {
        { VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position) },
        { VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord) },
    };

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));

    return true;
}

void OpenGLWindow::endRenderWindow()
{
    if (m_hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }
}

GLTexture *OpenGLWindow::getDecorationTexture() const
{
    if (AbstractClient *client = dynamic_cast<AbstractClient *>(toplevel)) {
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

WindowPixmap *OpenGLWindow::createWindowPixmap()
{
    return new OpenGLWindowPixmap(this, m_scene);
}

QVector4D OpenGLWindow::modulate(float opacity, float brightness) const
{
    const float a = opacity;
    const float rgb = opacity * brightness;

    return QVector4D(rgb, rgb, rgb, a);
}

void OpenGLWindow::setBlendEnabled(bool enabled)
{
    if (enabled && !m_blendingEnabled)
        glEnable(GL_BLEND);
    else if (!enabled && m_blendingEnabled)
        glDisable(GL_BLEND);

    m_blendingEnabled = enabled;
}

/**
 * \internal
 *
 * Counts the total number of pixmaps in the tree with the given root \a windowPixmap.
 */
static int windowPixmapCount(WindowPixmap *windowPixmap)
{
    int count = 1; // 1 for the window pixmap itself.

    const QVector<WindowPixmap *> children = windowPixmap->children();
    for (WindowPixmap *child : children)
        count += windowPixmapCount(child);

    return count;
}

void OpenGLWindow::initializeRenderContext(RenderContext &context, const WindowPaintData &data)
{
    WindowPixmap *currentPixmap = windowPixmap<OpenGLWindowPixmap>();

    context.shadowOffset = 0;
    context.decorationOffset = 1;
    context.contentOffset = 2;
    context.previousContentOffset = windowPixmapCount(currentPixmap) + 2;
    context.quadCount = data.quads.count();

    const int nodeCount = context.previousContentOffset + 1;

    QVector<RenderNode> &renderNodes = context.renderNodes;
    renderNodes.resize(nodeCount);

    for (const WindowQuad &quad : data.quads) {
        switch (quad.type()) {
        case WindowQuadShadow:
            renderNodes[context.shadowOffset].quads << quad;
            break;

        case WindowQuadDecoration:
            renderNodes[context.decorationOffset].quads << quad;
            break;

        case WindowQuadContents:
            renderNodes[context.contentOffset + quad.id()].quads << quad;
            break;

        default:
            // Ignore window quad generated by effects.
            break;
        }
    }

    RenderNode &shadowRenderNode = renderNodes[context.shadowOffset];
    if (!shadowRenderNode.quads.isEmpty()) {
        SceneOpenGLShadow *shadow = static_cast<SceneOpenGLShadow *>(m_shadow);
        shadowRenderNode.texture = shadow->shadowTexture();
        shadowRenderNode.opacity = data.opacity();
        shadowRenderNode.hasAlpha = true;
        shadowRenderNode.coordinateType = NormalizedCoordinates;
        shadowRenderNode.leafType = ShadowLeaf;
    }

    RenderNode &decorationRenderNode = renderNodes[context.decorationOffset];
    if (!decorationRenderNode.quads.isEmpty()) {
        decorationRenderNode.texture = getDecorationTexture();
        decorationRenderNode.opacity = data.opacity();
        decorationRenderNode.hasAlpha = true;
        decorationRenderNode.coordinateType = UnnormalizedCoordinates;
        decorationRenderNode.leafType = DecorationLeaf;
    }

    // FIXME: Cross-fading must be implemented in a shader.
    float contentOpacity = data.opacity();
    if (data.crossFadeProgress() != 1.0 && (data.opacity() < 0.95 || toplevel->hasAlpha())) {
        const float opacity = 1.0 - data.crossFadeProgress();
        contentOpacity *= 1 - pow(opacity, 1.0f + 2.0f * data.opacity());
    }

    // The main surface and all of its sub-surfaces form a tree. In order to initialize
    // the render nodes for the window pixmaps we need to traverse the tree in the
    // depth-first search manner. The id of content window quads corresponds to the time
    // when we visited the corresponding window pixmap. The DFS traversal probably doesn't
    // have a significant impact on performance. However, if that's the case, we could
    // keep a cache of window pixmaps in the order in which they'll be rendered.
    QStack<WindowPixmap *> stack;
    stack.push(currentPixmap);

    int i = 0;

    while (!stack.isEmpty()) {
        OpenGLWindowPixmap *windowPixmap = static_cast<OpenGLWindowPixmap *>(stack.pop());

        // If it's an unmapped sub-surface, don't render it and all of its children.
        if (!windowPixmap->isValid())
            continue;

        RenderNode &contentRenderNode = renderNodes[context.contentOffset + i++];
        contentRenderNode.texture = windowPixmap->texture();
        contentRenderNode.hasAlpha = windowPixmap->hasAlphaChannel();
        contentRenderNode.opacity = contentOpacity;
        contentRenderNode.coordinateType = UnnormalizedCoordinates;
        contentRenderNode.leafType = ContentLeaf;

        const QVector<WindowPixmap *> children = windowPixmap->children();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            stack.push(*it);
        }
    }

    // Note that cross-fading is currently working properly only on X11. In order to make it
    // work on Wayland, we have to render the current and the previous window pixmap trees in
    // offscreen render targets, then use a cross-fading shader to blend those two layers.
    if (data.crossFadeProgress() != 1.0) {
        OpenGLWindowPixmap *previous = previousWindowPixmap<OpenGLWindowPixmap>();
        if (previous) { // TODO(vlad): Should cross-fading be disabled on Wayland?
            const QRect &oldGeometry = previous->contentsRect();
            RenderNode &previousContentRenderNode = renderNodes[context.previousContentOffset];
            for (const WindowQuad &quad : qAsConst(renderNodes[context.contentOffset].quads)) {
                // We need to create new window quads with normalized texture coordinates.
                // Normal quads divide the x/y position by width/height. This would not work
                // as the texture is larger than the visible content in case of a decorated
                // Client resulting in garbage being shown. So we calculate the normalized
                // texture coordinate in the Client's new content space and map it to the
                // previous Client's content space.
                WindowQuad newQuad(WindowQuadContents);
                for (int i = 0; i < 4; ++i) {
                    const qreal xFactor = (quad[i].textureX() - toplevel->clientPos().x())
                            / qreal(toplevel->clientSize().width());
                    const qreal yFactor = (quad[i].textureY() - toplevel->clientPos().y())
                            / qreal(toplevel->clientSize().height());
                    const qreal u = (xFactor * oldGeometry.width() + oldGeometry.x())
                            / qreal(previous->size().width());
                    const qreal v = (yFactor * oldGeometry.height() + oldGeometry.y())
                            / qreal(previous->size().height());
                    newQuad[i] = WindowVertex(quad[i].x(), quad[i].y(), u, v);
                }
                previousContentRenderNode.quads.append(newQuad);
            }

            previousContentRenderNode.texture = previous->texture();
            previousContentRenderNode.hasAlpha = previous->hasAlphaChannel();
            previousContentRenderNode.opacity = data.opacity() * (1.0 - data.crossFadeProgress());
            previousContentRenderNode.coordinateType = NormalizedCoordinates;
            previousContentRenderNode.leafType = PreviousContentLeaf;

            context.quadCount += previousContentRenderNode.quads.count();
        }
    }
}

QMatrix4x4 OpenGLWindow::modelViewProjectionMatrix(int mask, const WindowPaintData &data) const
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

void OpenGLWindow::performPaint(int mask, const QRegion &region, const WindowPaintData &_data)
{
    WindowPaintData data = _data;
    if (!beginRenderWindow(mask, region, data))
        return;

    QMatrix4x4 windowMatrix = transformation(mask, data);
    const QMatrix4x4 modelViewProjection = modelViewProjectionMatrix(mask, data);
    const QMatrix4x4 mvpMatrix = modelViewProjection * windowMatrix;

    bool useX11TextureClamp = false;

    GLShader *shader = data.shader;
    GLenum filter;

    if (waylandServer()) {
        filter = GL_LINEAR;
    } else {
        const bool isTransformed = mask & (Effect::PAINT_WINDOW_TRANSFORMED |
                                           Effect::PAINT_SCREEN_TRANSFORMED);
        useX11TextureClamp = isTransformed;
        if (isTransformed && options->glSmoothScale() != 0) {
            filter = GL_LINEAR;
        } else {
            filter = GL_NEAREST;
        }
    }

    if (!shader) {
        ShaderTraits traits = ShaderTrait::MapTexture;
        if (useX11TextureClamp) {
            traits |= ShaderTrait::ClampTexture;
        }

        if (data.opacity() != 1.0 || data.brightness() != 1.0 || data.crossFadeProgress() != 1.0)
            traits |= ShaderTrait::Modulate;

        if (data.saturation() != 1.0)
            traits |= ShaderTrait::AdjustSaturation;

        shader = ShaderManager::instance()->pushShader(traits);
    }
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvpMatrix);

    shader->setUniform(GLShader::Saturation, data.saturation());

    RenderContext renderContext;
    initializeRenderContext(renderContext, data);

    const bool indexedQuads = GLVertexBuffer::supportsIndexedQuads();
    const GLenum primitiveType = indexedQuads ? GL_QUADS : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;

    const size_t size = verticesPerQuad * renderContext.quadCount * sizeof(GLVertex2D);

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    GLVertex2D *map = (GLVertex2D *) vbo->map(size);

    for (int i = 0, v = 0; i < renderContext.renderNodes.count(); i++) {
        RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.quads.isEmpty() || !renderNode.texture)
            continue;

        renderNode.firstVertex = v;
        renderNode.vertexCount = renderNode.quads.count() * verticesPerQuad;

        const QMatrix4x4 matrix = renderNode.texture->matrix(renderNode.coordinateType);

        renderNode.quads.makeInterleavedArrays(primitiveType, &map[v], matrix);
        v += renderNode.quads.count() * verticesPerQuad;
    }

    vbo->unmap();
    vbo->bindArrays();

    // Make sure the blend function is set up correctly in case we will be doing blending
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    float opacity = -1.0;

    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0)
            continue;

        setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);

        if (opacity != renderNode.opacity) {
            shader->setUniform(GLShader::ModulationConstant,
                               modulate(renderNode.opacity, data.brightness()));
            opacity = renderNode.opacity;
        }

        renderNode.texture->setFilter(filter);
        renderNode.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        renderNode.texture->bind();

        if (renderNode.leafType == ContentLeaf && useX11TextureClamp) {
            // X11 windows are reparented to have their buffer in the middle of a larger texture
            // holding the frame window.
            // This code passes the texture geometry to the fragment shader
            // any samples near the edge of the texture will be constrained to be
            // at least half a pixel in bounds, meaning we don't bleed the transparent border
            QRectF bufferContentRect = clientShape().boundingRect();
            bufferContentRect.adjust(0.5, 0.5, -0.5, -0.5);
            const QRect bufferGeometry = toplevel->bufferGeometry();

            float leftClamp = bufferContentRect.left() / bufferGeometry.width();
            float topClamp = bufferContentRect.top() / bufferGeometry.height();
            float rightClamp = bufferContentRect.right() / bufferGeometry.width();
            float bottomClamp = bufferContentRect.bottom() / bufferGeometry.height();
            shader->setUniform(GLShader::TextureClamp, QVector4D({leftClamp, topClamp, rightClamp, bottomClamp}));
        } else {
            shader->setUniform(GLShader::TextureClamp, QVector4D({0, 0, 1, 1}));
        }

        vbo->draw(region, primitiveType, renderNode.firstVertex,
                  renderNode.vertexCount, m_hardwareClipping);
    }

    vbo->unbindArrays();

    setBlendEnabled(false);

    if (!data.shader)
        ShaderManager::instance()->popShader();

    endRenderWindow();
}

QSharedPointer<GLTexture> OpenGLWindow::windowTexture()
{
    auto frame = windowPixmap<OpenGLWindowPixmap>();

    if (frame && frame->children().isEmpty()) {
        return QSharedPointer<GLTexture>(new GLTexture(*frame->texture()));
    } else {
        auto effectWindow = window()->effectWindow();
        const QRect geo = window()->clientGeometry();
        QSharedPointer<GLTexture> texture(new GLTexture(GL_RGBA8, geo.size()));

        QScopedPointer<GLRenderTarget> framebuffer(new KWin::GLRenderTarget(*texture));
        GLRenderTarget::pushRenderTarget(framebuffer.data());

        auto renderVSG = GLRenderTarget::virtualScreenGeometry();
        GLVertexBuffer::setVirtualScreenGeometry(geo);
        GLRenderTarget::setVirtualScreenGeometry(geo);

        QMatrix4x4 mvp;
        mvp.ortho(geo.x(), geo.x() + geo.width(), geo.y(), geo.y() + geo.height(), -1, 1);

        WindowPaintData data(effectWindow);
        data.setProjectionMatrix(mvp);

        performPaint(Scene::PAINT_WINDOW_TRANSFORMED | Scene::PAINT_WINDOW_LANCZOS, geo, data);
        GLRenderTarget::popRenderTarget();
        GLVertexBuffer::setVirtualScreenGeometry(renderVSG);
        GLRenderTarget::setVirtualScreenGeometry(renderVSG);
        return texture;
    }
}

//****************************************
// OpenGLWindowPixmap
//****************************************

OpenGLWindowPixmap::OpenGLWindowPixmap(Scene::Window *window, SceneOpenGL* scene)
    : WindowPixmap(window)
    , m_texture(scene->createTexture())
    , m_scene(scene)
{
}

OpenGLWindowPixmap::OpenGLWindowPixmap(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface, WindowPixmap *parent, SceneOpenGL *scene)
    : WindowPixmap(subSurface, parent)
    , m_texture(scene->createTexture())
    , m_scene(scene)
{
}

OpenGLWindowPixmap::~OpenGLWindowPixmap()
{
}

static bool needsPixmapUpdate(const OpenGLWindowPixmap *pixmap)
{
    // That's a regular Wayland client.
    if (pixmap->surface()) {
        return !pixmap->surface()->trackedDamage().isEmpty();
    }

    // That's an internal client with a raster buffer attached.
    if (!pixmap->internalImage().isNull()) {
        return !pixmap->toplevel()->damage().isEmpty();
    }

    // That's an internal client with an opengl framebuffer object attached.
    if (!pixmap->fbo().isNull()) {
        return !pixmap->toplevel()->damage().isEmpty();
    }

    // That's an X11 client.
    return false;
}

bool OpenGLWindowPixmap::bind()
{
    if (!m_texture->isNull()) {
        if (needsPixmapUpdate(this)) {
            m_texture->updateFromPixmap(this);
            // mipmaps need to be updated
            m_texture->setDirty();
        }
        if (subSurface().isNull()) {
            toplevel()->resetDamage();
        }
        // also bind all children
        for (auto it = children().constBegin(); it != children().constEnd(); ++it) {
            static_cast<OpenGLWindowPixmap*>(*it)->bind();
        }
        return true;
    }
    for (auto it = children().constBegin(); it != children().constEnd(); ++it) {
        static_cast<OpenGLWindowPixmap*>(*it)->bind();
    }
    if (!isValid()) {
        return false;
    }

    bool success = m_texture->load(this);

    if (success) {
        if (subSurface().isNull()) {
            toplevel()->resetDamage();
        }
    } else
        qCDebug(KWIN_OPENGL) << "Failed to bind window";
    return success;
}

WindowPixmap *OpenGLWindowPixmap::createChild(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface)
{
    return new OpenGLWindowPixmap(subSurface, this, m_scene);
}

bool OpenGLWindowPixmap::isValid() const
{
    if (!m_texture->isNull()) {
        return true;
    }
    return WindowPixmap::isValid();
}

//****************************************
// SceneOpenGL::EffectFrame
//****************************************

GLTexture* SceneOpenGL::EffectFrame::m_unstyledTexture = nullptr;
QPixmap* SceneOpenGL::EffectFrame::m_unstyledPixmap = nullptr;

SceneOpenGL::EffectFrame::EffectFrame(EffectFrameImpl* frame, SceneOpenGL *scene)
    : Scene::EffectFrame(frame)
    , m_texture(nullptr)
    , m_textTexture(nullptr)
    , m_oldTextTexture(nullptr)
    , m_textPixmap(nullptr)
    , m_iconTexture(nullptr)
    , m_oldIconTexture(nullptr)
    , m_selectionTexture(nullptr)
    , m_unstyledVBO(nullptr)
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
    m_texture = nullptr;
    delete m_textTexture;
    m_textTexture = nullptr;
    delete m_textPixmap;
    m_textPixmap = nullptr;
    delete m_iconTexture;
    m_iconTexture = nullptr;
    delete m_selectionTexture;
    m_selectionTexture = nullptr;
    delete m_unstyledVBO;
    m_unstyledVBO = nullptr;
    delete m_oldIconTexture;
    m_oldIconTexture = nullptr;
    delete m_oldTextTexture;
    m_oldTextTexture = nullptr;
}

void SceneOpenGL::EffectFrame::freeIconFrame()
{
    delete m_iconTexture;
    m_iconTexture = nullptr;
}

void SceneOpenGL::EffectFrame::freeTextFrame()
{
    delete m_textTexture;
    m_textTexture = nullptr;
    delete m_textPixmap;
    m_textPixmap = nullptr;
}

void SceneOpenGL::EffectFrame::freeSelection()
{
    delete m_selectionTexture;
    m_selectionTexture = nullptr;
}

void SceneOpenGL::EffectFrame::crossFadeIcon()
{
    delete m_oldIconTexture;
    m_oldIconTexture = m_iconTexture;
    m_iconTexture = nullptr;
}

void SceneOpenGL::EffectFrame::crossFadeText()
{
    delete m_oldTextTexture;
    m_oldTextTexture = m_textTexture;
    m_textTexture = nullptr;
}

void SceneOpenGL::EffectFrame::render(const QRegion &_region, double opacity, double frameOpacity)
{
    if (m_effectFrame->geometry().isEmpty())
        return; // Nothing to display

    Q_UNUSED(_region);
    const QRegion region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    GLShader* shader = m_effectFrame->shader();
    if (!shader) {
        shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture | ShaderTrait::Modulate);
    } else if (shader) {
        ShaderManager::instance()->pushShader(shader);
    }

    if (shader) {
        shader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));
        shader->setUniform(GLShader::Saturation, 1.0f);
    }
    const QMatrix4x4 projection = m_scene->projectionMatrix();

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

        QMatrix4x4 mvp(projection);
        mvp.translate(pt.x(), pt.y());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

        m_unstyledVBO->render(region, GL_TRIANGLES);
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
        const QRect rect = m_effectFrame->geometry().adjusted(-left, -top, right, bottom);

        QMatrix4x4 mvp(projection);
        mvp.translate(rect.x(), rect.y());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

        m_texture->render(region, rect);
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
            QMatrix4x4 mvp(projection);
            mvp.translate(m_effectFrame->selection().x(), m_effectFrame->selection().y());
            shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
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

        QMatrix4x4 mvp(projection);
        mvp.translate(topLeft.x(), topLeft.y());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

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
        QMatrix4x4 mvp(projection);
        mvp.translate(m_effectFrame->geometry().x(), m_effectFrame->geometry().y());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
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

        if (m_textTexture) {
            m_textTexture->bind();
            m_textTexture->render(region, m_effectFrame->geometry());
            m_textTexture->unbind();
        }
    }

    if (shader) {
        ShaderManager::instance()->popShader();
    }
    glDisable(GL_BLEND);
}

void SceneOpenGL::EffectFrame::updateTexture()
{
    delete m_texture;
    m_texture = nullptr;
    if (m_effectFrame->style() == EffectFrameStyled) {
        QPixmap pixmap = m_effectFrame->frame().framePixmap();
        m_texture = new GLTexture(pixmap);
    }
}

void SceneOpenGL::EffectFrame::updateTextTexture()
{
    delete m_textTexture;
    m_textTexture = nullptr;
    delete m_textPixmap;
    m_textPixmap = nullptr;

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
    m_unstyledTexture = nullptr;
    delete m_unstyledPixmap;
    m_unstyledPixmap = nullptr;
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
    m_unstyledTexture = nullptr;
    delete m_unstyledPixmap;
    m_unstyledPixmap = nullptr;
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
    const auto &decoShadow = shadow->decorationShadow().toStrongRef();
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
    Scene *scene = Compositor::self()->scene();
    if (scene) {
        scene->makeOpenGLContextCurrent();
        DecorationShadowTextureCache::instance().unregister(this);
        m_texture.reset();
    }
}

static inline void distributeHorizontally(QRectF &leftRect, QRectF &rightRect)
{
    if (leftRect.right() > rightRect.left()) {
        const qreal boundedRight = qMin(leftRect.right(), rightRect.right());
        const qreal boundedLeft = qMax(leftRect.left(), rightRect.left());
        const qreal halfOverlap = (boundedRight - boundedLeft) / 2.0;
        leftRect.setRight(boundedRight - halfOverlap);
        rightRect.setLeft(boundedLeft + halfOverlap);
    }
}

static inline void distributeVertically(QRectF &topRect, QRectF &bottomRect)
{
    if (topRect.bottom() > bottomRect.top()) {
        const qreal boundedBottom = qMin(topRect.bottom(), bottomRect.bottom());
        const qreal boundedTop = qMax(topRect.top(), bottomRect.top());
        const qreal halfOverlap = (boundedBottom - boundedTop) / 2.0;
        topRect.setBottom(boundedBottom - halfOverlap);
        bottomRect.setTop(boundedTop + halfOverlap);
    }
}

void SceneOpenGLShadow::buildQuads()
{
    // Do not draw shadows if window width or window height is less than
    // 5 px. 5 is an arbitrary choice.
    if (topLevel()->width() < 5 || topLevel()->height() < 5) {
        m_shadowQuads.clear();
        setShadowRegion(QRegion());
        return;
    }

    const QSizeF top(elementSize(ShadowElementTop));
    const QSizeF topRight(elementSize(ShadowElementTopRight));
    const QSizeF right(elementSize(ShadowElementRight));
    const QSizeF bottomRight(elementSize(ShadowElementBottomRight));
    const QSizeF bottom(elementSize(ShadowElementBottom));
    const QSizeF bottomLeft(elementSize(ShadowElementBottomLeft));
    const QSizeF left(elementSize(ShadowElementLeft));
    const QSizeF topLeft(elementSize(ShadowElementTopLeft));

    const QMarginsF shadowMargins(
        std::max({topLeft.width(), left.width(), bottomLeft.width()}),
        std::max({topLeft.height(), top.height(), topRight.height()}),
        std::max({topRight.width(), right.width(), bottomRight.width()}),
        std::max({bottomRight.height(), bottom.height(), bottomLeft.height()}));

    const QRectF outerRect(QPointF(-leftOffset(), -topOffset()),
                           QPointF(topLevel()->width() + rightOffset(),
                                   topLevel()->height() + bottomOffset()));

    const int width = shadowMargins.left() + std::max(top.width(), bottom.width()) + shadowMargins.right();
    const int height = shadowMargins.top() + std::max(left.height(), right.height()) + shadowMargins.bottom();

    QRectF topLeftRect;
    if (!topLeft.isEmpty()) {
        topLeftRect = QRectF(outerRect.topLeft(), topLeft);
    } else {
        topLeftRect = QRectF(
            outerRect.left() + shadowMargins.left(),
            outerRect.top() + shadowMargins.top(),
            0, 0);
    }

    QRectF topRightRect;
    if (!topRight.isEmpty()) {
        topRightRect = QRectF(
            outerRect.right() - topRight.width(), outerRect.top(),
            topRight.width(), topRight.height());
    } else {
        topRightRect = QRectF(
            outerRect.right() - shadowMargins.right(),
            outerRect.top() + shadowMargins.top(),
            0, 0);
    }

    QRectF bottomRightRect;
    if (!bottomRight.isEmpty()) {
        bottomRightRect = QRectF(
            outerRect.right() - bottomRight.width(),
            outerRect.bottom() - bottomRight.height(),
            bottomRight.width(), bottomRight.height());
    } else {
        bottomRightRect = QRectF(
            outerRect.right() - shadowMargins.right(),
            outerRect.bottom() - shadowMargins.bottom(),
            0, 0);
    }

    QRectF bottomLeftRect;
    if (!bottomLeft.isEmpty()) {
        bottomLeftRect = QRectF(
            outerRect.left(), outerRect.bottom() - bottomLeft.height(),
            bottomLeft.width(), bottomLeft.height());
    } else {
        bottomLeftRect = QRectF(
            outerRect.left() + shadowMargins.left(),
            outerRect.bottom() - shadowMargins.bottom(),
            0, 0);
    }

    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    distributeHorizontally(topLeftRect, topRightRect);
    distributeHorizontally(bottomLeftRect, bottomRightRect);
    distributeVertically(topLeftRect, bottomLeftRect);
    distributeVertically(topRightRect, bottomRightRect);

    qreal tx1 = 0.0,
          tx2 = 0.0,
          ty1 = 0.0,
          ty2 = 0.0;

    m_shadowQuads.clear();

    if (topLeftRect.isValid()) {
        tx1 = 0.0;
        ty1 = 0.0;
        tx2 = topLeftRect.width() / width;
        ty2 = topLeftRect.height() / height;
        WindowQuad topLeftQuad(WindowQuadShadow);
        topLeftQuad[0] = WindowVertex(topLeftRect.left(),  topLeftRect.top(),    tx1, ty1);
        topLeftQuad[1] = WindowVertex(topLeftRect.right(), topLeftRect.top(),    tx2, ty1);
        topLeftQuad[2] = WindowVertex(topLeftRect.right(), topLeftRect.bottom(), tx2, ty2);
        topLeftQuad[3] = WindowVertex(topLeftRect.left(),  topLeftRect.bottom(), tx1, ty2);
        m_shadowQuads.append(topLeftQuad);
    }

    if (topRightRect.isValid()) {
        tx1 = 1.0 - topRightRect.width() / width;
        ty1 = 0.0;
        tx2 = 1.0;
        ty2 = topRightRect.height() / height;
        WindowQuad topRightQuad(WindowQuadShadow);
        topRightQuad[0] = WindowVertex(topRightRect.left(),  topRightRect.top(),    tx1, ty1);
        topRightQuad[1] = WindowVertex(topRightRect.right(), topRightRect.top(),    tx2, ty1);
        topRightQuad[2] = WindowVertex(topRightRect.right(), topRightRect.bottom(), tx2, ty2);
        topRightQuad[3] = WindowVertex(topRightRect.left(),  topRightRect.bottom(), tx1, ty2);
        m_shadowQuads.append(topRightQuad);
    }

    if (bottomRightRect.isValid()) {
        tx1 = 1.0 - bottomRightRect.width() / width;
        tx2 = 1.0;
        ty1 = 1.0 - bottomRightRect.height() / height;
        ty2 = 1.0;
        WindowQuad bottomRightQuad(WindowQuadShadow);
        bottomRightQuad[0] = WindowVertex(bottomRightRect.left(),  bottomRightRect.top(),    tx1, ty1);
        bottomRightQuad[1] = WindowVertex(bottomRightRect.right(), bottomRightRect.top(),    tx2, ty1);
        bottomRightQuad[2] = WindowVertex(bottomRightRect.right(), bottomRightRect.bottom(), tx2, ty2);
        bottomRightQuad[3] = WindowVertex(bottomRightRect.left(),  bottomRightRect.bottom(), tx1, ty2);
        m_shadowQuads.append(bottomRightQuad);
    }

    if (bottomLeftRect.isValid()) {
        tx1 = 0.0;
        tx2 = bottomLeftRect.width() / width;
        ty1 = 1.0 - bottomLeftRect.height() / height;
        ty2 = 1.0;
        WindowQuad bottomLeftQuad(WindowQuadShadow);
        bottomLeftQuad[0] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.top(),    tx1, ty1);
        bottomLeftQuad[1] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.top(),    tx2, ty1);
        bottomLeftQuad[2] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.bottom(), tx2, ty2);
        bottomLeftQuad[3] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.bottom(), tx1, ty2);
        m_shadowQuads.append(bottomLeftQuad);
    }

    QRectF topRect(
        QPointF(topLeftRect.right(), outerRect.top()),
        QPointF(topRightRect.left(), outerRect.top() + top.height()));

    QRectF rightRect(
        QPointF(outerRect.right() - right.width(), topRightRect.bottom()),
        QPointF(outerRect.right(), bottomRightRect.top()));

    QRectF bottomRect(
        QPointF(bottomLeftRect.right(), outerRect.bottom() - bottom.height()),
        QPointF(bottomRightRect.left(), outerRect.bottom()));

    QRectF leftRect(
        QPointF(outerRect.left(), topLeftRect.bottom()),
        QPointF(outerRect.left() + left.width(), bottomLeftRect.top()));

    // Re-distribute left/right and top/bottom shadow tiles so they don't
    // overlap when the window is too small. Please notice that we don't
    // fix overlaps between left/top(left/bottom, right/top, and so on)
    // corner tiles because corresponding counter parts won't be valid when
    // the window is too small, which means they won't be rendered.
    distributeHorizontally(leftRect, rightRect);
    distributeVertically(topRect, bottomRect);

    if (topRect.isValid()) {
        tx1 = shadowMargins.left() / width;
        ty1 = 0.0;
        tx2 = tx1 + top.width() / width;
        ty2 = topRect.height() / height;
        WindowQuad topQuad(WindowQuadShadow);
        topQuad[0] = WindowVertex(topRect.left(),  topRect.top(),    tx1, ty1);
        topQuad[1] = WindowVertex(topRect.right(), topRect.top(),    tx2, ty1);
        topQuad[2] = WindowVertex(topRect.right(), topRect.bottom(), tx2, ty2);
        topQuad[3] = WindowVertex(topRect.left(),  topRect.bottom(), tx1, ty2);
        m_shadowQuads.append(topQuad);
    }

    if (rightRect.isValid()) {
        tx1 = 1.0 - rightRect.width() / width;
        ty1 = shadowMargins.top() / height;
        tx2 = 1.0;
        ty2 = ty1 + right.height() / height;
        WindowQuad rightQuad(WindowQuadShadow);
        rightQuad[0] = WindowVertex(rightRect.left(),  rightRect.top(),    tx1, ty1);
        rightQuad[1] = WindowVertex(rightRect.right(), rightRect.top(),    tx2, ty1);
        rightQuad[2] = WindowVertex(rightRect.right(), rightRect.bottom(), tx2, ty2);
        rightQuad[3] = WindowVertex(rightRect.left(),  rightRect.bottom(), tx1, ty2);
        m_shadowQuads.append(rightQuad);
    }

    if (bottomRect.isValid()) {
        tx1 = shadowMargins.left() / width;
        ty1 = 1.0 - bottomRect.height() / height;
        tx2 = tx1 + bottom.width() / width;
        ty2 = 1.0;
        WindowQuad bottomQuad(WindowQuadShadow);
        bottomQuad[0] = WindowVertex(bottomRect.left(),  bottomRect.top(),    tx1, ty1);
        bottomQuad[1] = WindowVertex(bottomRect.right(), bottomRect.top(),    tx2, ty1);
        bottomQuad[2] = WindowVertex(bottomRect.right(), bottomRect.bottom(), tx2, ty2);
        bottomQuad[3] = WindowVertex(bottomRect.left(),  bottomRect.bottom(), tx1, ty2);
        m_shadowQuads.append(bottomQuad);
    }

    if (leftRect.isValid()) {
        tx1 = 0.0;
        ty1 = shadowMargins.top() / height;
        tx2 = leftRect.width() / width;
        ty2 = ty1 + left.height() / height;
        WindowQuad leftQuad(WindowQuadShadow);
        leftQuad[0] = WindowVertex(leftRect.left(),  leftRect.top(),    tx1, ty1);
        leftQuad[1] = WindowVertex(leftRect.right(), leftRect.top(),    tx2, ty1);
        leftQuad[2] = WindowVertex(leftRect.right(), leftRect.bottom(), tx2, ty2);
        leftQuad[3] = WindowVertex(leftRect.left(),  leftRect.bottom(), tx1, ty2);
        m_shadowQuads.append(leftQuad);
    }
}

bool SceneOpenGLShadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        // simplifies a lot by going directly to
        Scene *scene = Compositor::self()->scene();
        scene->makeOpenGLContextCurrent();
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
    const QSize bottomRight(shadowPixmap(ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) +
                      std::max(top.width(), bottom.width()) +
                      std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) +
                       std::max(left.height(), right.height()) +
                       std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawPixmap(0, 0, topLeft.width(), topLeft.height(), shadowPixmap(ShadowElementTopLeft));
    p.drawPixmap(innerRectLeft, 0, top.width(), top.height(), shadowPixmap(ShadowElementTop));
    p.drawPixmap(width - topRight.width(), 0, topRight.width(), topRight.height(), shadowPixmap(ShadowElementTopRight));

    p.drawPixmap(0, innerRectTop, left.width(), left.height(), shadowPixmap(ShadowElementLeft));
    p.drawPixmap(width - right.width(), innerRectTop, right.width(), right.height(), shadowPixmap(ShadowElementRight));

    p.drawPixmap(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height(), shadowPixmap(ShadowElementBottomLeft));
    p.drawPixmap(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height(), shadowPixmap(ShadowElementBottom));
    p.drawPixmap(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height(), shadowPixmap(ShadowElementBottomRight));
    
    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    if (!GLPlatform::instance()->isGLES() && GLTexture::supportsSwizzle() && GLTexture::supportsFormatRG()) {
        QImage alphaImage(image.size(), QImage::Format_Indexed8); // Change to Format_Alpha8 w/ Qt 5.5
        bool alphaOnly = true;

        for (ptrdiff_t y = 0; alphaOnly && y < image.height(); y++) {
            const uint32_t * const src = reinterpret_cast<const uint32_t *>(image.scanLine(y));
            uint8_t * const dst = reinterpret_cast<uint8_t *>(alphaImage.scanLine(y));

            for (ptrdiff_t x = 0; x < image.width(); x++) {
                if (src[x] & 0x00ffffff)
                    alphaOnly = false;

                dst[x] = qAlpha(src[x]);
            }
        }

        if (alphaOnly) {
            image = alphaImage;
        }
    }

    Scene *scene = Compositor::self()->scene();
    scene->makeOpenGLContextCurrent();
    m_texture = QSharedPointer<GLTexture>::create(image);

    if (m_texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        m_texture->bind();
        m_texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }

    return true;
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : Renderer(client)
    , m_texture()
{
    connect(this, &Renderer::renderScheduled, client->client(), static_cast<void (AbstractClient::*)(const QRect&)>(&AbstractClient::addRepaint));
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer()
{
    if (Scene *scene = Compositor::self()->scene()) {
        scene->makeOpenGLContextCurrent();
    }
}

// Rotates the given source rect 90° counter-clockwise,
// and flips it vertically
static QImage rotate(const QImage &srcImage, const QRect &srcRect)
{
    auto dpr = srcImage.devicePixelRatio();
    QImage image(srcRect.height() * dpr, srcRect.width() * dpr, srcImage.format());
    image.setDevicePixelRatio(dpr);
    const QPoint srcPoint(srcRect.x() * dpr, srcRect.y() * dpr);

    const uint32_t *src = reinterpret_cast<const uint32_t *>(srcImage.bits());
    uint32_t *dst = reinterpret_cast<uint32_t *>(image.bits());

    for (int x = 0; x < image.width(); x++) {
        const uint32_t *s = src + (srcPoint.y() + x) * srcImage.width() + srcPoint.x();
        uint32_t *d = dst + x;

        for (int y = 0; y < image.height(); y++) {
            *d = s[y];
            d += image.width();
        }
    }

    return image;
}

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const QRect &viewport)
{
    Q_ASSERT(image.depth() == 32);

    const QRect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
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

    if (!m_texture) {
        // for invalid sizes we get no texture, see BUG 361551
        return;
    }

    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    // We pad each part in the decoration atlas in order to avoid texture bleeding.
    const int padding = 1;

    auto renderPart = [=](const QRect &geo, const QRect &partRect, const QPoint &position, bool rotated = false) {
        if (!geo.isValid()) {
            return;
        }

        QRect rect = geo;

        // We allow partial decoration updates and it might just so happen that the dirty region
        // is completely contained inside the decoration part, i.e. the dirty region doesn't touch
        // any of the decoration's edges. In that case, we should **not** pad the dirty region.
        if (rect.left() == partRect.left()) {
            rect.setLeft(rect.left() - padding);
        }
        if (rect.top() == partRect.top()) {
            rect.setTop(rect.top() - padding);
        }
        if (rect.right() == partRect.right()) {
            rect.setRight(rect.right() + padding);
        }
        if (rect.bottom() == partRect.bottom()) {
            rect.setBottom(rect.bottom() + padding);
        }

        QRect viewport = geo.translated(-rect.x(), -rect.y());
        const qreal devicePixelRatio = client()->client()->screenScale();

        QImage image(rect.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setViewport(QRect(viewport.topLeft(), viewport.size() * devicePixelRatio));
        painter.setWindow(QRect(geo.topLeft(), geo.size() * devicePixelRatio));
        painter.setClipRect(geo);
        renderToPainter(&painter, geo);
        painter.end();

        clamp(image, QRect(viewport.topLeft(), viewport.size() * devicePixelRatio));

        if (rotated) {
            // TODO: get this done directly when rendering to the image
            image = rotate(image, QRect(QPoint(), rect.size()));
            viewport = QRect(viewport.y(), viewport.x(), viewport.height(), viewport.width());
        }

        const QPoint dirtyOffset = geo.topLeft() - partRect.topLeft();
        m_texture->update(image, (position + dirtyOffset - viewport.topLeft()) * image.devicePixelRatio());
    };

    const QRect geometry = scheduled.boundingRect();

    const QPoint topPosition(padding, padding);
    const QPoint bottomPosition(padding, topPosition.y() + top.height() + 2 * padding);
    const QPoint leftPosition(padding, bottomPosition.y() + bottom.height() + 2 * padding);
    const QPoint rightPosition(padding, leftPosition.y() + left.width() + 2 * padding);

    renderPart(left.intersected(geometry), left, leftPosition, true);
    renderPart(top.intersected(geometry), top, topPosition);
    renderPart(right.intersected(geometry), right, rightPosition, true);
    renderPart(bottom.intersected(geometry), bottom, bottomPosition);
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
                     left.width() + right.width();

    // Reserve some space for padding. We pad decoration parts to avoid texture bleeding.
    const int padding = 1;
    size.rwidth() += 2 * padding;
    size.rheight() += 4 * 2 * padding;

    size.rwidth() = align(size.width(), 128);

    size *= client()->client()->screenScale();
    if (m_texture && m_texture->size() == size)
        return;

    if (!size.isEmpty()) {
        m_texture.reset(new GLTexture(GL_RGBA8, size.width(), size.height()));
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


OpenGLFactory::OpenGLFactory(QObject *parent)
    : SceneFactory(parent)
{
}

OpenGLFactory::~OpenGLFactory() = default;

Scene *OpenGLFactory::create(QObject *parent) const
{
    qCDebug(KWIN_OPENGL) << "Initializing OpenGL compositing";

    // Some broken drivers crash on glXQuery() so to prevent constant KWin crashes:
    if (kwinApp()->platform()->openGLCompositingIsBroken()) {
        qCWarning(KWIN_OPENGL) << "KWin has detected that your OpenGL library is unsafe to use";
        return nullptr;
    }
    kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PreInit);
    auto s = SceneOpenGL::createScene(parent);
    kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PostInit);
    if (s && s->initFailed()) {
        delete s;
        return nullptr;
    }
    return s;
}

} // namespace
