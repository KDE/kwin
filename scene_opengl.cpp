/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.

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
#include "config-workspace.h"
#ifdef WAYLAND_FOUND
#include "egl_wayland_backend.h"
#endif
#endif
#ifndef KWIN_HAVE_OPENGLES
#include "glxbackend.h"
#endif

#include <kxerrorhandler.h>

#include <kwinglcolorcorrection.h>
#include <kwinglplatform.h>

#include "utils.h"
#include "client.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "overlaywindow.h"
#include "paintredirector.h"
#include "screens.h"
#include "workspace.h"

#include <cmath>
#include <unistd.h>
#include <stddef.h>

// turns on checks for opengl errors in various places (for easier finding of them)
// normally only few of them are enabled
//#define CHECK_GL_ERROR

#include <X11/extensions/Xcomposite.h>

#include <qpainter.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QGraphicsScale>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

#include <KDE/KLocalizedString>
#include <KDE/KNotification>
#include <KProcess>

namespace KWin
{

extern int currentRefreshRate();

//****************************************
// SceneOpenGL
//****************************************
OpenGLBackend::OpenGLBackend()
    : m_overlayWindow(new OverlayWindow()) // TODO: maybe create only if needed?
    , m_syncsToVBlank(false)
    , m_blocksForRetrace(false)
    , m_directRendering(false)
    , m_haveBufferAge(false)
    , m_failed(false)
{
}

OpenGLBackend::~OpenGLBackend()
{
    if (isFailed()) {
        m_overlayWindow->destroy();
    }
    delete m_overlayWindow;
}

void OpenGLBackend::setFailed(const QString &reason)
{
    kWarning(1212) << "Creating the OpenGL rendering failed: " << reason;
    m_failed = true;
}

void OpenGLBackend::idle()
{
    if (hasPendingFlush())
        present();
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
        region = QRegion(0, 0, displayWidth(), displayHeight());
    }

    return region;
}

/************************************************
 * SceneOpenGL
 ***********************************************/

SceneOpenGL::SceneOpenGL(Workspace* ws, OpenGLBackend *backend)
    : Scene(ws)
    , init_ok(true)
    , m_backend(backend)
{
    if (m_backend->isFailed()) {
        init_ok = false;
        return;
    }
    if (!viewportLimitsMatched(QSize(displayWidth(), displayHeight())))
        return;

    // perform Scene specific checks
    GLPlatform *glPlatform = GLPlatform::instance();
#ifndef KWIN_HAVE_OPENGLES
    if (!hasGLExtension("GL_ARB_texture_non_power_of_two")
            && !hasGLExtension("GL_ARB_texture_rectangle")) {
        kError(1212) << "GL_ARB_texture_non_power_of_two and GL_ARB_texture_rectangle missing";
        init_ok = false;
        return; // error
    }
#endif
    if (glPlatform->isMesaDriver() && glPlatform->mesaVersion() < kVersionNumber(8, 0)) {
        kError(1212) << "KWin requires at least Mesa 8.0 for OpenGL compositing.";
        init_ok = false;
        return;
    }
#ifndef KWIN_HAVE_OPENGLES
    glDrawBuffer(GL_BACK);
#endif

    m_debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!glPlatform->supports(LooseBinding));
    }
}

SceneOpenGL::~SceneOpenGL()
{
    foreach (Window * w, windows) {
        delete w;
    }
    // do cleanup after initBuffer()
    SceneOpenGL::EffectFrame::cleanup();
    if (init_ok) {
        // backend might be still needed for a different scene
        delete m_backend;
    }
}

SceneOpenGL *SceneOpenGL::createScene()
{
    OpenGLBackend *backend = NULL;
    OpenGLPlatformInterface platformInterface = NoOpenGLPlatformInterface;
    // should we use glx?
#ifndef KWIN_HAVE_OPENGLES
    // on OpenGL we default to glx
    platformInterface = GlxPlatformInterface;
#endif

    const QByteArray envOpenGLInterface(qgetenv("KWIN_OPENGL_INTERFACE"));
#ifdef KWIN_HAVE_EGL
#ifdef KWIN_HAVE_OPENGLES
    // for OpenGL ES we need to use the Egl Backend
    platformInterface = EglPlatformInterface;
#else
    // check environment variable
    if (qstrcmp(envOpenGLInterface, "egl") == 0 ||
            qstrcmp(envOpenGLInterface, "egl_wayland") == 0) {
        kDebug(1212) << "Forcing EGL native interface through environment variable";
        platformInterface = EglPlatformInterface;
    }
#endif
#endif

    switch (platformInterface) {
    case GlxPlatformInterface:
#ifndef KWIN_HAVE_OPENGLES
        backend = new GlxBackend();
#endif
        break;
    case EglPlatformInterface:
#ifdef KWIN_HAVE_EGL
#ifdef WAYLAND_FOUND
        if (qstrcmp(envOpenGLInterface, "egl_wayland") == 0) {
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
#ifdef KWIN_HAVE_OPENGL_1
    if (SceneOpenGL1::supported(backend)) {
        scene = new SceneOpenGL1(backend);
        if (scene->initFailed()) {
            delete scene;
            scene = NULL;
        }
    }
#endif
    if (!scene) {
        if (GLPlatform::instance()->recommendedCompositor() == XRenderCompositing) {
            kError(1212) << "OpenGL driver recommends XRender based compositing. Falling back to XRender.";
            kError(1212) << "To overwrite the detection use the environment variable KWIN_COMPOSE";
            kError(1212) << "For more information see http://community.kde.org/KWin/Environment_Variables#KWIN_COMPOSE";
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
    foreach (const QRect &r, region.rects()) {
        const int x0 = r.x();
        const int y0 = displayHeight() - r.y() - r.height();
        const int x1 = r.x() + r.width();
        const int y1 = displayHeight() - r.y();

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
        kDebug(1212) << "A graphics reset attributable to the current GL context occurred.";
        break;

    case GL_INNOCENT_CONTEXT_RESET_KWIN:
        kDebug(1212) << "A graphics reset not attributable to the current GL context occurred.";
        break;

    case GL_UNKNOWN_CONTEXT_RESET_KWIN:
        kDebug(1212) << "A graphics reset of an unknown cause occurred.";
        break;

    default:
        break;
    }

    QElapsedTimer timer;
    timer.start();

    // Wait until the reset is completed or max 10 seconds
    while (timer.elapsed() < 10000 && glGetGraphicsResetStatus() != GL_NO_ERROR)
        usleep(50);

    kDebug(1212) << "Attempting to reset compositing.";
    QMetaObject::invokeMethod(this, "resetCompositing", Qt::QueuedConnection);

    KNotification::event("graphicsreset", i18n("Desktop effects were restarted due to a graphics reset"));
}

qint64 SceneOpenGL::paint(QRegion damage, ToplevelList toplevels)
{
    // actually paint the frame, flushed with the NEXT frame
    foreach (Toplevel * c, toplevels) {
        // TODO: cache the stacking_order in case it has not changed
        assert(windows.contains(c));
        stacking_order.append(windows[ c ]);
    }

    QRegion repaint = m_backend->prepareRenderingFrame();

    const GLenum status = glGetGraphicsResetStatus();
    if (status != GL_NO_ERROR) {
        handleGraphicsReset(status);
        return 0;
    }

    int mask = 0;
#ifdef CHECK_GL_ERROR
    checkGLError("Paint1");
#endif

    // After this call, updateRegion will contain the damaged region in the
    // back buffer. This is the region that needs to be posted to repair
    // the front buffer. It doesn't include the additional damage returned
    // by prepareRenderingFrame(). validRegion is the region that has been
    // repainted, and may be larger than updateRegion.
    QRegion updateRegion, validRegion;
    paintScreen(&mask, damage, repaint, &updateRegion, &validRegion);   // call generic implementation

#ifndef KWIN_HAVE_OPENGLES
    const QRegion displayRegion(0, 0, displayWidth(), displayHeight());

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

#ifdef CHECK_GL_ERROR
    checkGLError("Paint2");
#endif

    m_backend->endRenderingFrame(validRegion, updateRegion);

    // do cleanup
    stacking_order.clear();
    checkGLError("PostPaint");
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

    if (options->glPreferBufferSwap() == Options::ExtendDamage) { // only Extend "large" repaints
        const QRegion displayRegion(0, 0, displayWidth(), displayHeight());
        uint damagedPixels = 0;
        const uint fullRepaintLimit = (opaqueFullscreen?0.49f:0.748f)*displayWidth()*displayHeight();
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
        region = QRegion(0, 0, displayWidth(), displayHeight());
    }
}

void SceneOpenGL::windowAdded(Toplevel* c)
{
    assert(!windows.contains(c));
    Window *w = createWindow(c);
    windows[ c ] = w;
    w->setScene(this);
    connect(c, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), SLOT(windowOpacityChanged(KWin::Toplevel*)));
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SLOT(windowGeometryShapeChanged(KWin::Toplevel*)));
    connect(c, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), SLOT(windowClosed(KWin::Toplevel*,KWin::Deleted*)));
    c->effectWindow()->setSceneWindow(windows[ c ]);
    c->getShadow();
    windows[ c ]->updateShadow(c->shadow());
}

void SceneOpenGL::windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted)
{
    assert(windows.contains(c));
    if (deleted != NULL) {
        // replace c with deleted
        Window* w = windows.take(c);
        w->updateToplevel(deleted);
        if (w->shadow()) {
            w->shadow()->setToplevel(deleted);
        }
        windows[ deleted ] = w;
    } else {
        delete windows.take(c);
        c->effectWindow()->setSceneWindow(NULL);
    }
}

void SceneOpenGL::windowDeleted(Deleted* c)
{
    assert(windows.contains(c));
    delete windows.take(c);
    c->effectWindow()->setSceneWindow(NULL);
}

void SceneOpenGL::windowGeometryShapeChanged(KWin::Toplevel* c)
{
    if (!windows.contains(c))    // this is ok, shape is not valid
        return;                 // by default
    Window* w = windows[ c ];
    w->discardShape();
}

void SceneOpenGL::windowOpacityChanged(KWin::Toplevel* t)
{
    Q_UNUSED(t)
#if 0 // not really needed, windows are painted on every repaint
    // and opacity is used when applying texture, not when
    // creating it
    if (!windows.contains(c))    // this is ok, texture is created
        return;                 // on demand
    Window* w = windows[ c ];
    w->discardTexture();
#endif
}

SceneOpenGL::Texture *SceneOpenGL::createTexture()
{
    return new Texture(m_backend);
}

SceneOpenGL::Texture *SceneOpenGL::createTexture(const QPixmap &pix, GLenum target)
{
    return new Texture(m_backend, pix, target);
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
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.kwinCompositingDialog").value()) {
            QDBusInterface dialog( "org.kde.kwinCompositingDialog", "/CompositorSettings", "org.kde.kwinCompositingDialog" );
            dialog.asyncCall("warn", message, details, "");
        } else {
            const QString args = "warn " + message.toLocal8Bit().toBase64() + " details " + details.toLocal8Bit().toBase64();
            KProcess::startDetached("kcmshell4", QStringList() << "kwincompositing" << "--args" << args);
        }
        QDBusConnection::sessionBus().interface()->setTimeout(oldTimeout);
        return false;
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, limit);
    if (limit[0] < size.width() || limit[0] < size.height()) {
        KConfig cfg("kwin_dialogsrc");

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
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.kwinCompositingDialog").value()) {
            QDBusInterface dialog( "org.kde.kwinCompositingDialog", "/CompositorSettings", "org.kde.kwinCompositingDialog" );
            dialog.asyncCall("warn", message, details, "kwin_dialogsrc:max_tex_warning");
        } else {
            const QString args = "warn " + message.toLocal8Bit().toBase64() + " details " +
                                 details.toLocal8Bit().toBase64() + " dontagain kwin_dialogsrc:max_tex_warning";
            KProcess::startDetached("kcmshell4", QStringList() << "kwincompositing" << "--args" << args);
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
    ShaderManager::instance()->resetAllShaders();
}

void SceneOpenGL::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    const QRect r = region.boundingRect();
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x(), displayHeight() - r.y() - r.height(), r.width(), r.height());
    KWin::Scene::paintDesktop(desktop, mask, region, data);
    glDisable(GL_SCISSOR_TEST);
}

//****************************************
// SceneOpenGL2
//****************************************
bool SceneOpenGL2::supported(OpenGLBackend *backend)
{
    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O2") == 0) {
            kDebug(1212) << "OpenGL 2 compositing enforced by environment variable";
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
        kDebug(1212) << "Driver does not recommend OpenGL 2 compositing";
#ifndef KWIN_HAVE_OPENGLES
        return false;
#endif
    }
    if (options->isGlLegacy()) {
        kDebug(1212) << "OpenGL 2 disabled by config option";
        return false;
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

    if (!ShaderManager::instance()->isValid()) {
        kDebug(1212) << "No Scene Shaders available";
        init_ok = false;
        return;
    }

    // push one shader on the stack so that one is always bound
    ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
    if (checkGLError("Init")) {
        kError(1212) << "OpenGL 2 compositing setup failed";
        init_ok = false;
        return; // error
    }

    kDebug(1212) << "OpenGL 2 compositing successfully initialized";

#ifndef KWIN_HAVE_OPENGLES
    // It is not legal to not have a vertex array object bound in a core context
    if (hasGLExtension("GL_ARB_vertex_array_object")) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }
#endif

    init_ok = true;
}

SceneOpenGL2::~SceneOpenGL2()
{
}

void SceneOpenGL2::paintGenericScreen(int mask, ScreenPaintData data)
{
    ShaderBinder binder(ShaderManager::GenericShader);

    binder.shader()->setUniform(GLShader::ScreenTransformation, transformation(mask, data));

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

SceneOpenGL::Window *SceneOpenGL2::createWindow(Toplevel *t)
{
    return new SceneOpenGL2Window(t);
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
    kDebug(1212) << "Color correction:" << options->isColorCorrected();
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
// SceneOpenGL1
//****************************************
#ifdef KWIN_HAVE_OPENGL_1
bool SceneOpenGL1::supported(OpenGLBackend *backend)
{
    Q_UNUSED(backend)
    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O1") == 0) {
            kDebug(1212) << "OpenGL 1 compositing enforced by environment variable";
            return true;
        } else {
            // OpenGL 1 disabled by environment variable
            return false;
        }
    }
    if (GLPlatform::instance()->recommendedCompositor() < OpenGL1Compositing) {
        kDebug(1212) << "Driver does not recommend OpenGL 1 compositing";
        return false;
    }
    return true;
}

SceneOpenGL1::SceneOpenGL1(OpenGLBackend *backend)
    : SceneOpenGL(Workspace::self(), backend)
    , m_resetModelViewProjectionMatrix(true)
{
    if (!init_ok) {
        // base ctor already failed
        return;
    }
    ShaderManager::disable();
    setupModelViewProjectionMatrix();
    if (checkGLError("Init")) {
        kError(1212) << "OpenGL 1 compositing setup failed";
        init_ok = false;
        return; // error
    }

    kDebug(1212) << "OpenGL 1 compositing successfully initialized";
}

SceneOpenGL1::~SceneOpenGL1()
{
}

qint64 SceneOpenGL1::paint(QRegion damage, ToplevelList windows)
{
    if (m_resetModelViewProjectionMatrix) {
        // reset model view projection matrix if required
        setupModelViewProjectionMatrix();
    }
    return SceneOpenGL::paint(damage, windows);
}

void SceneOpenGL1::paintGenericScreen(int mask, ScreenPaintData data)
{
    pushMatrix(transformation(mask, data));
    Scene::paintGenericScreen(mask, data);
    popMatrix();
}

void SceneOpenGL1::doPaintBackground(const QVector< float >& vertices)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setData(vertices.count() / 2, 2, vertices.data(), NULL);
    vbo->render(GL_TRIANGLES);
}

void SceneOpenGL1::setupModelViewProjectionMatrix()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float fovy = 60.0f;
    float aspect = 1.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float ymax = zNear * tan(fovy  * M_PI / 360.0f);
    float ymin = -ymax;
    float xmin =  ymin * aspect;
    float xmax = ymax * aspect;
    // swap top and bottom to have OpenGL coordinate system match X system
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float scaleFactor = 1.1 * tan(fovy * M_PI / 360.0f) / ymax;
    glTranslatef(xmin * scaleFactor, ymax * scaleFactor, -1.1);
    glScalef((xmax - xmin)*scaleFactor / displayWidth(), -(ymax - ymin)*scaleFactor / displayHeight(), 0.001);
    m_resetModelViewProjectionMatrix = false;
}

void SceneOpenGL1::screenGeometryChanged(const QSize &size)
{
    SceneOpenGL::screenGeometryChanged(size);
    m_resetModelViewProjectionMatrix = true;
}

SceneOpenGL::Window *SceneOpenGL1::createWindow(Toplevel *t)
{
    return new SceneOpenGL1Window(t);
}

#endif

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::Texture::Texture(OpenGLBackend *backend)
    : GLTexture(*backend->createBackendTexture(this))
{
}

SceneOpenGL::Texture::Texture(OpenGLBackend *backend, const QPixmap &pix, GLenum target)
    : GLTexture(*backend->createBackendTexture(this))
{
    load(pix, target);
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

bool SceneOpenGL::Texture::load(const Pixmap& pix, const QSize& size,
                                int depth)
{
    if (pix == None)
        return false;
    return load(pix, size, depth,
                QRegion(0, 0, size.width(), size.height()));
}

bool SceneOpenGL::Texture::load(const QImage& image, GLenum target)
{
    if (image.isNull())
        return false;
    return load(QPixmap::fromImage(image), target);
}

bool SceneOpenGL::Texture::load(const QPixmap& pixmap, GLenum target)
{
    if (pixmap.isNull())
        return false;

    // Checking whether QPixmap comes with its own X11 Pixmap
    if (Extensions::nonNativePixmaps()) {
        return GLTexture::load(pixmap.toImage(), target);
    }

    // use the X11 pixmap provided by Qt
    return load(pixmap.handle(), pixmap.size(), pixmap.depth());
}

void SceneOpenGL::Texture::findTarget()
{
    Q_D(Texture);
    d->findTarget();
}

bool SceneOpenGL::Texture::load(const Pixmap& pix, const QSize& size,
                                int depth, QRegion region)
{
    Q_UNUSED(region)
    // decrease the reference counter for the old texture
    d_ptr = d_func()->backend()->createBackendTexture(this); //new TexturePrivate();

    Q_D(Texture);
    return d->loadTexture(pix, size, depth);
}

bool SceneOpenGL::Texture::update(const QRegion &damage)
{
    Q_D(Texture);
    return d->update(damage);
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

bool SceneOpenGL::TexturePrivate::update(const QRegion &damage)
{
    Q_UNUSED(damage)
    return true;
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
                // case 1: completely contains, include and do not check other rects
                if (rf.contains(quadRect)) {
                    quads << quad;
                    break;
                }
                // case 2: intersection
                if (rf.intersects(quadRect)) {
                    const QRectF intersected = rf.intersected(quadRect);
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


OpenGLPaintRedirector *SceneOpenGL::Window::paintRedirector() const
{
    if (toplevel->isClient()) {
        Client *client = static_cast<Client *>(toplevel);
        if (client->noBorder())
            return 0;

        return static_cast<OpenGLPaintRedirector *>(client->decorationPaintRedirector());
    }

    if (toplevel->isDeleted()) {
        Deleted *deleted = static_cast<Deleted *>(toplevel);
        if (deleted->noBorder())
            return 0;

        return static_cast<OpenGLPaintRedirector *>(deleted->decorationPaintRedirector());
    }

    return 0;
}

bool SceneOpenGL::Window::getDecorationTextures(GLTexture **textures) const
{
    OpenGLPaintRedirector *redirector = paintRedirector();
    if (!redirector)
        return false;

    redirector->ensurePixmapsPainted();

    textures[0] = redirector->leftRightTexture();
    textures[1] = redirector->topBottomTexture();

    redirector->markAsRepainted();
    return true;
}

void SceneOpenGL::Window::paintDecorations(const WindowPaintData &data, const QRegion &region)
{
    GLTexture *textures[2];
    if (!getDecorationTextures(textures))
        return;

    WindowQuadList quads[2]; // left-right, top-bottom

    // Split the quads into two lists
    foreach (const WindowQuad &quad, data.quads) {
        switch (quad.type()) {
        case WindowQuadDecorationLeftRight:
            quads[0].append(quad);
            continue;

        case WindowQuadDecorationTopBottom:
            quads[1].append(quad);
            continue;

        default:
            continue;
        }
    }

    TextureType type[] = { DecorationLeftRight, DecorationTopBottom };
    for (int i = 0; i < 2; i++)
        paintDecoration(textures[i], type[i], region, data, quads[i]);
}

void SceneOpenGL::Window::paintDecoration(GLTexture *texture, TextureType type,
                                          const QRegion &region, const WindowPaintData &data,
                                          const WindowQuadList &quads)
{
    if (!texture || quads.isEmpty())
        return;

    if (filter == ImageFilterGood)
        texture->setFilter(GL_LINEAR);
    else
        texture->setFilter(GL_NEAREST);

    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->bind();

    prepareStates(type, data.opacity() * data.decorationOpacity(), data.brightness(), data.saturation(), data.screen());
    renderQuads(0, region, quads, texture, false);
    restoreStates(type, data.opacity() * data.decorationOpacity(), data.brightness(), data.saturation());

    texture->unbind();

#ifndef KWIN_HAVE_OPENGLES
    if (m_scene && m_scene->debug()) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GLVertexBuffer::streamingBuffer()->render(region, GL_TRIANGLES, m_hardwareClipping);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
}

void SceneOpenGL::Window::paintShadow(const QRegion &region, const WindowPaintData &data)
{
    WindowQuadList quads;

    foreach (const WindowQuad &quad, data.quads) {
        switch (quad.type()) {
        case WindowQuadShadowTopLeft:
        case WindowQuadShadowTop:
        case WindowQuadShadowTopRight:
        case WindowQuadShadowLeft:
        case WindowQuadShadowRight:
        case WindowQuadShadowBottomLeft:
        case WindowQuadShadowBottom:
        case WindowQuadShadowBottomRight:
             quads.append(quad);
             break;

        default:
             break;
        }
    }

    if (quads.isEmpty())
        return;

    GLTexture *texture = static_cast<SceneOpenGLShadow*>(m_shadow)->shadowTexture();
    if (!texture) {
        return;
    }
    if (filter == ImageFilterGood)
        texture->setFilter(GL_LINEAR);
    else
        texture->setFilter(GL_NEAREST);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->bind();
    prepareStates(Shadow, data.opacity(), data.brightness(), data.saturation(), data.screen());
    renderQuads(0, region, quads, texture, true);
    restoreStates(Shadow, data.opacity(), data.brightness(), data.saturation());
    texture->unbind();
#ifndef KWIN_HAVE_OPENGLES
    if (m_scene && m_scene->debug()) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        renderQuads(0, region, quads, texture, true);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
}

void SceneOpenGL::Window::renderQuads(int, const QRegion& region, const WindowQuadList& quads,
                                      GLTexture *tex, bool normalized)
{
    if (quads.isEmpty())
        return;

    const QMatrix4x4 matrix = tex->matrix(normalized ? NormalizedCoordinates : UnnormalizedCoordinates);

    // Render geometry
    GLenum primitiveType;
    int primcount;

    if (GLVertexBuffer::supportsIndexedQuads()) {
        primitiveType = GL_QUADS_KWIN;
        primcount = quads.count() * 4;
    } else {
        primitiveType = GL_TRIANGLES;
        primcount = quads.count() * 6;
    }

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->setVertexCount(primcount);

    GLVertex2D *map = (GLVertex2D *) vbo->map(primcount * sizeof(GLVertex2D));
    quads.makeInterleavedArrays(primitiveType, map, matrix);
    vbo->unmap();

    vbo->render(region, primitiveType, m_hardwareClipping);
}

GLTexture *SceneOpenGL::Window::textureForType(SceneOpenGL::Window::TextureType type)
{
    GLTexture *tex = NULL;
    OpenGLPaintRedirector *redirector = NULL;

    if (type != Content && type != Shadow) {
        if (toplevel->isClient()) {
            Client *client = static_cast<Client*>(toplevel);
            redirector = static_cast<OpenGLPaintRedirector*>(client->decorationPaintRedirector());
        } else if (toplevel->isDeleted()) {
            Deleted *deleted = static_cast<Deleted*>(toplevel);
            redirector = static_cast<OpenGLPaintRedirector*>(deleted->decorationPaintRedirector());
        }
    }

    switch(type) {
    case Content:
        tex = s_frameTexture;
        break;

    case DecorationLeftRight:
        tex = redirector ? redirector->leftRightTexture() : 0;
        break;

    case DecorationTopBottom:
        tex = redirector ? redirector->topBottomTexture() : 0;
        break;

    case Shadow:
        tex = static_cast<SceneOpenGLShadow*>(m_shadow)->shadowTexture();
    }
    return tex;
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

    if (!quads[LeftRightLeaf].isEmpty() || !quads[TopBottomLeaf].isEmpty()) {
        GLTexture *textures[2];
        getDecorationTextures(textures);

        nodes[LeftRightLeaf].texture = textures[0];
        nodes[LeftRightLeaf].opacity = data.opacity();
        nodes[LeftRightLeaf].hasAlpha = true;
        nodes[LeftRightLeaf].coordinateType = UnnormalizedCoordinates;

        nodes[TopBottomLeaf].texture = textures[1];
        nodes[TopBottomLeaf].opacity = data.opacity();
        nodes[TopBottomLeaf].hasAlpha = true;
        nodes[TopBottomLeaf].coordinateType = UnnormalizedCoordinates;
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

void SceneOpenGL2Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (!beginRenderWindow(mask, region, data))
        return;

    GLShader *shader = data.shader;
    if (!shader) {
        if ((mask & Scene::PAINT_WINDOW_TRANSFORMED) || (mask & Scene::PAINT_SCREEN_TRANSFORMED)) {
            shader = ShaderManager::instance()->pushShader(ShaderManager::GenericShader);
        } else {
            shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
            shader->setUniform(GLShader::Offset, QVector2D(x(), y()));
        }
    }

    if (ColorCorrection *cc = static_cast<SceneOpenGL2*>(m_scene)->colorCorrection()) {
        cc->setupForOutput(data.screen());
    }

    shader->setUniform(GLShader::WindowTransformation, transformation(mask, data));
    shader->setUniform(GLShader::Saturation, data.saturation());

    const GLenum filter = (mask & (Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_SCREEN_TRANSFORMED))
                           && options->glSmoothScale() != 0 ? GL_LINEAR : GL_NEAREST;

    WindowQuadList quads[LeafCount];

    // Split the quads into separate lists for each type
    foreach (const WindowQuad &quad, data.quads) {
        switch (quad.type()) {
        case WindowQuadDecorationLeftRight:
            quads[LeftRightLeaf].append(quad);
            continue;

        case WindowQuadDecorationTopBottom:
            quads[TopBottomLeaf].append(quad);
            continue;

        case WindowQuadContents:
            quads[ContentLeaf].append(quad);
            continue;

        case WindowQuadShadowTopLeft:
        case WindowQuadShadowTop:
        case WindowQuadShadowTopRight:
        case WindowQuadShadowLeft:
        case WindowQuadShadowRight:
        case WindowQuadShadowBottomLeft:
        case WindowQuadShadowBottom:
        case WindowQuadShadowBottomRight:
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
            Q_FOREACH (const WindowQuad &quad, quads[ContentLeaf]) {
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
    const GLenum primitiveType = indexedQuads ? GL_QUADS_KWIN : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;

    const size_t size = verticesPerQuad *
        (quads[0].count() + quads[1].count() + quads[2].count() + quads[3].count() + quads[4].count()) * sizeof(GLVertex2D);

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

void SceneOpenGL2Window::prepareStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen)
{
    // setup blending of transparent windows
    bool opaque = isOpaque() && opacity == 1.0;
    bool alpha = toplevel->hasAlpha() || type != Content;
    if (type != Content) {
        if (type == Shadow) {
            opaque = false;
        } else {
            if (opacity == 1.0 && toplevel->isClient()) {
                opaque = !(static_cast<Client*>(toplevel)->decorationHasAlpha());
            } else {
                // TODO: add support in Deleted
                opaque = false;
            }
        }
    }
    if (!opaque) {
        glEnable(GL_BLEND);
        if (alpha) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glBlendColor((float)opacity, (float)opacity, (float)opacity, (float)opacity);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_CONSTANT_ALPHA);
        }
    }
    m_blendingEnabled = !opaque;

    const qreal rgb = brightness * opacity;
    const qreal a = opacity;

    GLShader *shader = ShaderManager::instance()->getBoundShader();
    shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::Saturation,         saturation);

    if (ColorCorrection *cc = static_cast<SceneOpenGL2*>(m_scene)->colorCorrection()) {
        cc->setupForOutput(screen);
    }
}

void SceneOpenGL2Window::restoreStates(TextureType type, qreal opacity, qreal brightness, qreal saturation)
{
    Q_UNUSED(type);
    Q_UNUSED(opacity);
    Q_UNUSED(brightness);
    Q_UNUSED(saturation);
    if (m_blendingEnabled) {
        glDisable(GL_BLEND);
    }

    if (ColorCorrection *cc = static_cast<SceneOpenGL2*>(m_scene)->colorCorrection()) {
        cc->setupForOutput(-1);
    }
}

//***************************************
// SceneOpenGL1Window
//***************************************
#ifdef KWIN_HAVE_OPENGL_1
SceneOpenGL1Window::SceneOpenGL1Window(Toplevel *c)
    : SceneOpenGL::Window(c)
{
}

SceneOpenGL1Window::~SceneOpenGL1Window()
{
}

// paint the window
void SceneOpenGL1Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (!beginRenderWindow(mask, region, data))
        return;

    pushMatrix(transformation(mask, data));

    // shadow
    if (m_shadow) {
        paintShadow(region, data);
    }
    // decorations
    paintDecorations(data, region);

    // paint the content
    OpenGLWindowPixmap *previous = previousWindowPixmap<OpenGLWindowPixmap>();
    const WindowQuadList contentQuads = data.quads.select(WindowQuadContents);
    if (previous && data.crossFadeProgress() != 1.0) {
        // TODO: ARGB crsoofading is atm. a hack, playing on opacities for two dumb SrcOver operations
        // Will require a caching texture or sth. else 1.2 compliant
        float opacity = data.opacity();
        if (opacity < 0.95f || toplevel->hasAlpha()) {
            opacity = 1 - data.crossFadeProgress();
            opacity = data.opacity() * (1 - pow(opacity, 1.0f + 2.0f * data.opacity()));
        }
        paintContent(s_frameTexture, region, mask, opacity, data, contentQuads, false);
        previous->texture()->setFilter(filter == Scene::ImageFilterGood ? GL_LINEAR : GL_NEAREST);
        WindowQuadList oldContents;
        const QRect &oldGeometry = previous->contentsRect();
        Q_FOREACH (const WindowQuad &quad, contentQuads) {
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
            oldContents.append(newQuad);
        }
        opacity = data.opacity() * (1.0 - data.crossFadeProgress());
        paintContent(previous->texture(), region, mask, opacity, data, oldContents, true);
    } else {
        paintContent(s_frameTexture, region, mask, data.opacity(), data, contentQuads, false);
    }

    popMatrix();

    endRenderWindow();
}

void SceneOpenGL1Window::paintContent(SceneOpenGL::Texture* content, const QRegion& region, int mask,
                                      qreal opacity, const WindowPaintData& data, const WindowQuadList &contentQuads, bool normalized)
{
    if (contentQuads.isEmpty()) {
        return;
    }
    content->bind();
    prepareStates(Content, opacity, data.brightness(), data.saturation(), data.screen());
    renderQuads(mask, region, contentQuads, content, normalized);
    restoreStates(Content, opacity, data.brightness(), data.saturation());
    content->unbind();
#ifndef KWIN_HAVE_OPENGLES
    if (m_scene && m_scene->debug()) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        renderQuads(mask, region, contentQuads, content, normalized);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
}

void SceneOpenGL1Window::prepareStates(TextureType type, qreal opacity, qreal brightness, qreal saturation, int screen)
{
    Q_UNUSED(screen)

    GLTexture *tex = textureForType(type);
    bool alpha = false;
    bool opaque = true;
    if (type == Content) {
        alpha = toplevel->hasAlpha();
        opaque = isOpaque() && opacity == 1.0;
    } else {
        alpha = true;
        opaque = false;
    }
    // setup blending of transparent windows
    glPushAttrib(GL_ENABLE_BIT);
    if (!opaque) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (saturation != 1.0 && tex->saturationSupported()) {
        // First we need to get the color from [0; 1] range to [0.5; 1] range
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        const float scale_constant[] = { 1.0, 1.0, 1.0, 0.5};
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, scale_constant);
        tex->bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture(GL_TEXTURE1);
        float saturation_constant[] = { 0.5 + 0.5 * 0.30, 0.5 + 0.5 * 0.59, 0.5 + 0.5 * 0.11,
                                        static_cast<float>(saturation) };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        tex->bind();

        // Finally we need to interpolate between the original image and the
        //  greyscale image to get wanted level of saturation
        glActiveTexture(GL_TEXTURE2);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        // Also replace alpha by primary color's alpha here
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        // And make primary color contain the wanted opacity
        glColor4f(opacity, opacity, opacity, opacity);
        tex->bind();

        if (alpha || !opaque || brightness != 1.0f) {
            glActiveTexture(GL_TEXTURE3);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            // The color has to be multiplied by both opacity and brightness
            float opacityByBrightness = opacity * brightness;
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity);
            if (alpha) {
                // Multiply original texture's alpha by our opacity
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            } else {
                // Alpha will be taken from previous stage
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            }
            tex->bind();
        }

        glActiveTexture(GL_TEXTURE0);
    } else if (opacity != 1.0 || brightness != 1.0) {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        float opacityByBrightness = opacity * brightness;
        if (alpha) {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness,
                      opacity);
        } else {
            // Multiply color by brightness and replace alpha by opacity
            float constant[] = { opacityByBrightness, opacityByBrightness, opacityByBrightness,
                                 static_cast<float>(opacity) };
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);
        }
    } else if (!alpha && opaque) {
        float constant[] = { 1.0, 1.0, 1.0, 1.0 };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);
    }
}

void SceneOpenGL1Window::restoreStates(TextureType type, qreal opacity, qreal brightness, qreal saturation)
{
    GLTexture *tex = textureForType(type);
    if (opacity != 1.0 || saturation != 1.0 || brightness != 1.0f) {
        if (saturation != 1.0 && tex->saturationSupported()) {
            glActiveTexture(GL_TEXTURE3);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE2);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE1);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE0);
        }
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(0, 0, 0, 0);

    glPopAttrib();  // ENABLE_BIT
}
#endif

//****************************************
// OpenGLWindowPixmap
//****************************************

OpenGLWindowPixmap::OpenGLWindowPixmap(Scene::Window *window, SceneOpenGL* scene)
    : WindowPixmap(window)
    , m_scene(scene)
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
            const bool success = m_texture->update(toplevel()->damage());
            // mipmaps need to be updated
            m_texture->setDirty();
            toplevel()->resetDamage();
            return success;
        }
        return true;
    }
    if (!isValid()) {
        return false;
    }

    bool success = m_texture->load(pixmap(), toplevel()->size(), toplevel()->depth(), toplevel()->damage());

    if (success)
        toplevel()->resetDamage();
    else
        kDebug(1212) << "Failed to bind window";
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
    if (!shader && ShaderManager::instance()->isValid()) {
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
#ifdef KWIN_HAVE_OPENGL_1
    if (!shader)
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

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
#ifdef KWIN_HAVE_OPENGL_1
        else
            glColor4f(0.0, 0.0, 0.0, opacity * frameOpacity);
#endif

        m_unstyledTexture->bind();
        const QPoint pt = m_effectFrame->geometry().topLeft();
        if (sceneShader) {
            shader->setUniform(GLShader::Offset, QVector2D(pt.x(), pt.y()));
        } else {
            QMatrix4x4 translation;
            translation.translate(pt.x(), pt.y());
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, translation);
            } else {
                pushMatrix(translation);
            }
        }
        m_unstyledVBO->render(region, GL_TRIANGLES);
        if (!sceneShader) {
            if (shader) {
                shader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
            } else {
                popMatrix();
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
#ifdef KWIN_HAVE_OPENGL_1
        else
            glColor4f(1.0, 1.0, 1.0, opacity * frameOpacity);
#endif
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
                m_selectionTexture = m_scene->createTexture(pixmap);
        }
        if (m_selectionTexture) {
            if (shader) {
                const float a = opacity * frameOpacity;
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
    #ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity * frameOpacity);
    #endif
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
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity * (1.0 - m_effectFrame->crossFadeProgress()));
#endif

            m_oldIconTexture->bind();
            m_oldIconTexture->render(region, QRect(topLeft, m_effectFrame->iconSize()));
            m_oldIconTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress());
#endif
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity);
#endif
        }

        if (!m_iconTexture) { // lazy creation
            m_iconTexture = m_scene->createTexture(m_effectFrame->icon());
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
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity *(1.0 - m_effectFrame->crossFadeProgress()));
#endif

            m_oldTextTexture->bind();
            m_oldTextTexture->render(region, m_effectFrame->geometry());
            m_oldTextTexture->unbind();
            if (shader) {
                const float a = opacity * m_effectFrame->crossFadeProgress();
                shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
            }
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress());
#endif
        } else {
            if (shader) {
                const QVector4D constant(opacity, opacity, opacity, opacity);
                shader->setUniform(GLShader::ModulationConstant, constant);
            }
#ifdef KWIN_HAVE_OPENGL_1
            else
                glColor4f(1.0, 1.0, 1.0, opacity);
#endif
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
        m_texture = m_scene->createTexture(pixmap);
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
    m_textTexture = m_scene->createTexture(*m_textPixmap);
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
SceneOpenGLShadow::SceneOpenGLShadow(Toplevel *toplevel)
    : Shadow(toplevel)
    , m_texture(NULL)
{
}

SceneOpenGLShadow::~SceneOpenGLShadow()
{
    delete m_texture;
}

void SceneOpenGLShadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QSizeF top(shadowPixmap(ShadowElementTop).size());
    const QSizeF topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSizeF right(shadowPixmap(ShadowElementRight).size());
    const QSizeF bottomRight(shadowPixmap(ShadowElementBottomRight).size());
    const QSizeF bottom(shadowPixmap(ShadowElementBottom).size());
    const QSizeF bottomLeft(shadowPixmap(ShadowElementBottomLeft).size());
    const QSizeF left(shadowPixmap(ShadowElementLeft).size());
    const QSizeF topLeft(shadowPixmap(ShadowElementTopLeft).size());
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
    WindowQuad topLeftQuad(WindowQuadShadowTopLeft);
    topLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                      outerRect.y(), tx1, ty1);
    topLeftQuad[ 1 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y(), tx2, ty1);
    topLeftQuad[ 2 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y() + topLeft.height(), tx2, ty2);
    topLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                      outerRect.y() + topLeft.height(), tx1, ty2);
    m_shadowQuads.append(topLeftQuad);

    tx1 = tx2;
    tx2 = (topLeft.width() + top.width())/width;
    ty2 = top.height()/height;
    WindowQuad topQuad(WindowQuadShadowTop);
    topQuad[ 0 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y(), tx1, ty1);
    topQuad[ 1 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y(), tx2, ty1);
    topQuad[ 2 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y() + top.height(),tx2, ty2);
    topQuad[ 3 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y() + top.height(), tx1, ty2);
    m_shadowQuads.append(topQuad);

    tx1 = tx2;
    tx2 = 1.0;
    ty2 = topRight.height()/height;
    WindowQuad topRightQuad(WindowQuadShadowTopRight);
    topRightQuad[ 0 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y(), tx1, ty1);
    topRightQuad[ 1 ] = WindowVertex(outerRect.right(),                     outerRect.y(), tx2, ty1);
    topRightQuad[ 2 ] = WindowVertex(outerRect.right(),                     outerRect.y() + topRight.height(), tx2, ty2);
    topRightQuad[ 3 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y() + topRight.height(), tx1, ty2);
    m_shadowQuads.append(topRightQuad);

    tx1 = (width - right.width())/width;
    ty1 = topRight.height()/height;
    ty2 = (topRight.height() + right.height())/height;
    WindowQuad rightQuad(WindowQuadShadowRight);
    rightQuad[ 0 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.y() + topRight.height(), tx1, ty1);
    rightQuad[ 1 ] = WindowVertex(outerRect.right(),                    outerRect.y() + topRight.height(), tx2, ty1);
    rightQuad[ 2 ] = WindowVertex(outerRect.right(),                    outerRect.bottom() - bottomRight.height(), tx2, ty2);
    rightQuad[ 3 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty2);
    m_shadowQuads.append(rightQuad);

    tx1 = (width - bottomRight.width())/width;
    ty1 = ty2;
    ty2 = 1.0;
    WindowQuad bottomRightQuad(WindowQuadShadowBottomRight);
    bottomRightQuad[ 0 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom() - bottomRight.height(), tx1, ty1);
    bottomRightQuad[ 1 ] = WindowVertex(outerRect.right(),                          outerRect.bottom() - bottomRight.height(), tx2, ty1);
    bottomRightQuad[ 2 ] = WindowVertex(outerRect.right(),                          outerRect.bottom(), tx2, ty2);
    bottomRightQuad[ 3 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomRightQuad);

    tx2 = tx1;
    tx1 = bottomLeft.width()/width;
    ty1 = (height - bottom.height())/height;
    WindowQuad bottomQuad(WindowQuadShadowBottom);
    bottomQuad[ 0 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom() - bottom.height(), tx1, ty1);
    bottomQuad[ 1 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom() - bottom.height(), tx2, ty1);
    bottomQuad[ 2 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom(), tx2, ty2);
    bottomQuad[ 3 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomQuad);

    tx1 = 0.0;
    tx2 = bottomLeft.width()/width;
    ty1 = (height - bottomLeft.height())/height;
    WindowQuad bottomLeftQuad(WindowQuadShadowBottomLeft);
    bottomLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                       outerRect.bottom() - bottomLeft.height(), tx1, ty1);
    bottomLeftQuad[ 1 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty1);
    bottomLeftQuad[ 2 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom(), tx2, ty2);
    bottomLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                       outerRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomLeftQuad);

    tx2 = left.width()/width;
    ty2 = ty1;
    ty1 = topLeft.height()/height;
    WindowQuad leftQuad(WindowQuadShadowLeft);
    leftQuad[ 0 ] = WindowVertex(outerRect.x(),                 outerRect.y() + topLeft.height(), tx1, ty1);
    leftQuad[ 1 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.y() + topLeft.height(), tx2, ty1);
    leftQuad[ 2 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.bottom() - bottomLeft.height(), tx2, ty2);
    leftQuad[ 3 ] = WindowVertex(outerRect.x(),                 outerRect.bottom() - bottomLeft.height(), tx1, ty2);
    m_shadowQuads.append(leftQuad);
}

bool SceneOpenGLShadow::prepareBackend()
{
    const QSize top(shadowPixmap(ShadowElementTop).size());
    const QSize topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSize right(shadowPixmap(ShadowElementRight).size());
    const QSize bottomRight(shadowPixmap(ShadowElementBottomRight).size());
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

    delete m_texture;
    m_texture = new GLTexture(image);

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
        kDebug(1212) << "Triple buffering detection:" << QString(blocks ? "NOT available" : "Available") <<
                        " - Mean block time:" << m_time/(1000.0*1000.0) << "ms";
        return blocks ? 'd' : 't';
    }
    return 0;
}

} // namespace
