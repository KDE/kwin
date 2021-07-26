/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_opengl.h"
#include "platformopenglsurfacetexture.h"

#include "platform.h"
#include "wayland_server.h"

#include <kwinglplatform.h>
#include <kwineffectquickview.h>

#include "utils.h"
#include "abstract_client.h"
#include "composite.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "main.h"
#include "overlaywindow.h"
#include "renderloop.h"
#include "screens.h"
#include "cursor.h"
#include "decorations/decoratedclient.h"
#include "shadowitem.h"
#include "surfaceitem.h"
#include "windowitem.h"
#include <logging.h>

#include <cmath>
#include <cstddef>
#include <unistd.h>

#include <QGraphicsScale>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

#include <KLocalizedString>
#include <KNotification>
#include <KProcess>
#include <QtMath>

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

/************************************************
 * SceneOpenGL
 ***********************************************/

SceneOpenGL::SceneOpenGL(OpenGLBackend *backend, QObject *parent)
    : Scene(parent)
    , init_ok(true)
    , m_backend(backend)
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
}

SceneOpenGL::~SceneOpenGL()
{
    if (init_ok) {
        makeOpenGLContextCurrent();
    }
    SceneOpenGL::EffectFrame::cleanup();

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
    QScopedPointer<OpenGLBackend> backend(kwinApp()->platform()->createOpenGLBackend());
    if (!backend) {
        return nullptr;
    }
    if (!backend->isFailed()) {
        backend->init();
    }
    if (backend->isFailed()) {
        return nullptr;
    }
    SceneOpenGL *scene = nullptr;
    // first let's try an OpenGL 2 scene
    if (SceneOpenGL2::supported(backend.data())) {
        scene = new SceneOpenGL2(backend.take(), parent);
        if (scene->initFailed()) {
            delete scene;
            scene = nullptr;
        } else {
            return scene;
        }
    }
    if (!scene) {
        if (GLPlatform::instance()->recommendedCompositor() == QPainterCompositing) {
            qCCritical(KWIN_OPENGL) << "OpenGL driver recommends QPainter based compositing. Falling back to QPainter.";
            qCCritical(KWIN_OPENGL) << "To overwrite the detection use the environment variable KWIN_COMPOSE";
            qCCritical(KWIN_OPENGL) << "For more information see https://community.kde.org/KWin/Environment_Variables#KWIN_COMPOSE";
        }
    }

    return scene;
}

OverlayWindow *SceneOpenGL::overlayWindow() const
{
    return m_backend->overlayWindow();
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

    m_resetOccurred = true;
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

    auto newTexture = [this] {
        const QImage img = Cursors::self()->currentCursor()->image();
        if (img.isNull()) {
            m_cursorTextureDirty = false;
            return;
        }
        m_cursorTexture.reset(new GLTexture(img));
        m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_cursorTextureDirty = false;
    };

    // lazy init texture cursor only in case we need software rendering
    if (!m_cursorTexture) {
        newTexture();

        // handle shape update on case cursor image changed
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, [this] {
            m_cursorTextureDirty = true;
        });
    } else if (m_cursorTextureDirty) {
        const QImage image = Cursors::self()->currentCursor()->image();
        if (image.size() == m_cursorTexture->size()) {
            m_cursorTexture->update(image);
            m_cursorTextureDirty = false;
        } else {
            newTexture();
        }
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

void SceneOpenGL::aboutToStartPainting(int screenId, const QRegion &damage)
{
    m_backend->aboutToStartPainting(screenId, damage);
}

static SurfaceItem *findTopMostSurface(SurfaceItem *item)
{
    const QList<Item *> children = item->childItems();
    if (children.isEmpty()) {
        return item;
    } else {
        return findTopMostSurface(static_cast<SurfaceItem *>(children.constLast()));
    }
}

void SceneOpenGL::paint(int screenId, const QRegion &damage, const QList<Toplevel *> &toplevels,
                        RenderLoop *renderLoop)
{
    if (m_resetOccurred) {
        return; // A graphics reset has occurred, do nothing.
    }

    painted_screen = screenId;
    // actually paint the frame, flushed with the NEXT frame
    createStackingOrder(toplevels);

    QRegion update;
    QRegion valid;
    QRegion repaint;
    QRect geo;
    qreal scaling;
    if (screenId != -1) {
        geo = screens()->geometry(screenId);
        scaling = screens()->scale(screenId);
    } else {
        geo = screens()->geometry();
        scaling = 1;
    }

    const GLenum status = glGetGraphicsResetStatus();
    if (status != GL_NO_ERROR) {
        handleGraphicsReset(status);
    } else {
        renderLoop->beginFrame();

        SurfaceItem *fullscreenSurface = nullptr;
        for (int i = stacking_order.count() - 1; i >=0; i--) {
            Window *window = stacking_order[i];
            Toplevel *toplevel = window->window();
            if (toplevel->isOnScreen(screenId) && window->isVisible() && toplevel->opacity() > 0) {
                AbstractClient *c = dynamic_cast<AbstractClient*>(toplevel);
                if (!c || !c->isFullScreen()) {
                    break;
                }
                if (!window->surfaceItem()) {
                    break;
                }
                SurfaceItem *topMost = findTopMostSurface(window->surfaceItem());
                auto pixmap = topMost->pixmap();
                if (!pixmap) {
                    break;
                }
                pixmap->update();
                // the subsurface has to be able to cover the whole window
                if (topMost->position() != QPoint(0, 0)) {
                    break;
                }
                // and it has to be completely opaque
                if (!window->isOpaque() && !topMost->opaque().contains(QRect(0, 0, window->width(), window->height()))) {
                    break;
                }
                fullscreenSurface = topMost;
                break;
            }
        }
        renderLoop->setFullscreenSurface(fullscreenSurface);

        bool directScanout = false;
        if (m_backend->directScanoutAllowed(screenId) && !static_cast<EffectsHandlerImpl*>(effects)->blocksDirectScanout()) {
            directScanout = m_backend->scanout(screenId, fullscreenSurface);
        }
        if (directScanout) {
            renderLoop->endFrame();
        } else {
            // prepare rendering makescontext current on the output
            repaint = m_backend->beginFrame(screenId);

            GLVertexBuffer::setVirtualScreenGeometry(geo);
            GLRenderTarget::setVirtualScreenGeometry(geo);
            GLVertexBuffer::setVirtualScreenScale(scaling);
            GLRenderTarget::setVirtualScreenScale(scaling);

            updateProjectionMatrix(geo);

            paintScreen(damage.intersected(geo), repaint, &update, &valid,
                        renderLoop, projectionMatrix());   // call generic implementation
            paintCursor(valid);

            if (!GLPlatform::instance()->isGLES() && screenId == -1) {
                const QSize &screenSize = screens()->size();
                const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());

                // copy dirty parts from front to backbuffer
                if (!m_backend->supportsBufferAge() &&
                    options->glPreferBufferSwap() == Options::CopyFrontBuffer &&
                    valid != displayRegion) {
                    glReadBuffer(GL_FRONT);
                    m_backend->copyPixels(displayRegion - valid);
                    glReadBuffer(GL_BACK);
                    valid = displayRegion;
                }
            }

            renderLoop->endFrame();

            GLVertexBuffer::streamingBuffer()->endOfFrame();
            m_backend->endFrame(screenId, valid, update);
            GLVertexBuffer::streamingBuffer()->framePosted();
        }
    }

    // do cleanup
    clearStackingOrder();
}

QMatrix4x4 SceneOpenGL::transformation(int mask, const ScreenPaintData &data) const
{
    QMatrix4x4 matrix;

    if (!(mask & PAINT_SCREEN_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    const QVector3D scale = data.scale();
    matrix.scale(scale.x(), scale.y(), scale.z());

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
    if (region == infiniteRegion()) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (!region.isEmpty()) {
        QVector<float> verts;
        verts.reserve(region.rectCount() * 6 * 2);

        for (const QRect &r : region) {
            verts << r.x() + r.width() << r.y();
            verts << r.x() << r.y();
            verts << r.x() << r.y() + r.height();
            verts << r.x() << r.y() + r.height();
            verts << r.x() + r.width() << r.y() + r.height();
            verts << r.x() + r.width() << r.y();
        }
        doPaintBackground(verts);
    }
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

bool SceneOpenGL::supportsNativeFence() const
{
    return m_backend->supportsNativeFence();
}

Scene::EffectFrame *SceneOpenGL::createEffectFrame(EffectFrameImpl *frame)
{
    return new SceneOpenGL::EffectFrame(frame, this);
}

Shadow *SceneOpenGL::createShadow(Toplevel *toplevel)
{
    return new SceneOpenGLShadow(toplevel);
}

DecorationRenderer *SceneOpenGL::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
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

PlatformSurfaceTexture *SceneOpenGL::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backend->createPlatformSurfaceTextureInternal(pixmap);
}

PlatformSurfaceTexture *SceneOpenGL::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backend->createPlatformSurfaceTextureWayland(pixmap);
}

PlatformSurfaceTexture *SceneOpenGL::createPlatformSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return m_backend->createPlatformSurfaceTextureX11(pixmap);
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
    if (GLPlatform::instance()->recommendedCompositor() < OpenGLCompositing) {
        qCDebug(KWIN_OPENGL) << "Driver does not recommend OpenGL compositing";
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

void SceneOpenGL2::updateProjectionMatrix(const QRect &rect)
{
    // Create a perspective projection with a 60° field-of-view,
    // and an aspect ratio of 1.0.
    m_projectionMatrix.setToIdentity();
    const float fovY   =   std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect =    1.0f;
    const float zNear  =    0.1f;
    const float zFar   =  100.0f;

    const float yMax   =  zNear * fovY;
    const float yMin   = -yMax;
    const float xMin   =  yMin * aspect;
    const float xMax   =  yMax * aspect;

    m_projectionMatrix.frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    const float scaleFactor = 1.1 * fovY / yMax;
    m_projectionMatrix.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    m_projectionMatrix.scale( (xMax - xMin) * scaleFactor / rect.width(),
                             -(yMax - yMin) * scaleFactor / rect.height(),
                              0.001);
    m_projectionMatrix.translate(-rect.x(), -rect.y());
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

static QMatrix4x4 transformation(const Item *item, const OpenGLWindow::RenderContext *context)
{
    const QPoint position = item->rootPosition();
    QMatrix4x4 matrix;
    matrix.translate(position.x(), position.y());

    if (!(context->paintFlags & Scene::PAINT_WINDOW_TRANSFORMED))
        return matrix;

    const WindowPaintData *data = &context->paintData;
    matrix.translate(data->translation());
    const QVector3D scale = data->scale();
    matrix.scale(scale.x(), scale.y(), scale.z());

    if (data->rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // cannot use data.rotation.applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data->rotationOrigin());
    const QVector3D axis = data->rotationAxis();
    matrix.rotate(data->rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data->rotationOrigin());

    return matrix;
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

static GLTexture *bindSurfaceTexture(SurfaceItem *surfaceItem)
{
    SurfacePixmap *surfacePixmap = surfaceItem->pixmap();
    auto platformSurfaceTexture =
            static_cast<PlatformOpenGLSurfaceTexture *>(surfacePixmap->platformTexture());
    if (surfacePixmap->isDiscarded()) {
        return platformSurfaceTexture->texture();
    }

    if (platformSurfaceTexture->texture()) {
        const QRegion region = surfaceItem->damage();
        if (!region.isEmpty()) {
            platformSurfaceTexture->update(region);
            surfaceItem->resetDamage();
        }
    } else {
        if (!surfacePixmap->isValid()) {
            return nullptr;
        }
        if (!platformSurfaceTexture->create()) {
            qCDebug(KWIN_OPENGL) << "Failed to bind window";
            return nullptr;
        }
        surfaceItem->resetDamage();
    }

    return platformSurfaceTexture->texture();
}

static WindowQuadList clipQuads(const Item *item, const OpenGLWindow::RenderContext *context)
{
    const WindowQuadList quads = item->quads();
    if (context->clip != infiniteRegion() && !context->hardwareClipping) {
        const QPoint offset = item->rootPosition();

        WindowQuadList ret;
        ret.reserve(quads.count());

        // split all quads in bounding rect with the actual rects in the region
        for (const WindowQuad &quad : qAsConst(quads)) {
            for (const QRect &r : qAsConst(context->clip)) {
                const QRectF rf(r.translated(-offset));
                const QRectF quadRect(QPointF(quad.left(), quad.top()), QPointF(quad.right(), quad.bottom()));
                const QRectF &intersected = rf.intersected(quadRect);
                if (intersected.isValid()) {
                    if (quadRect == intersected) {
                        // case 1: completely contains, include and do not check other rects
                        ret << quad;
                        break;
                    }
                    // case 2: intersection
                    ret << quad.makeSubQuad(intersected.left(), intersected.top(), intersected.right(), intersected.bottom());
                }
            }
        }
        return ret;
    }
    return quads;
}

void OpenGLWindow::createRenderNode(Item *item, RenderContext *context)
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->isVisible()) {
            createRenderNode(childItem, context);
        }
    }

    item->preprocess();
    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        WindowQuadList quads = clipQuads(item, context);
        if (!quads.isEmpty()) {
            SceneOpenGLShadow *shadow = static_cast<SceneOpenGLShadow *>(shadowItem->shadow());
            context->renderNodes.append(RenderNode{
                .texture = shadow->shadowTexture(),
                .quads = quads,
                .transformMatrix = transformation(item, context),
                .opacity = context->paintData.opacity(),
                .hasAlpha = true,
                .coordinateType = NormalizedCoordinates,
            });
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        WindowQuadList quads = clipQuads(item, context);
        if (!quads.isEmpty()) {
            auto renderer = static_cast<const SceneOpenGLDecorationRenderer *>(decorationItem->renderer());
            context->renderNodes.append(RenderNode{
                .texture = renderer->texture(),
                .quads = quads,
                .transformMatrix = transformation(item, context),
                .opacity = context->paintData.opacity(),
                .hasAlpha = true,
                .coordinateType = UnnormalizedCoordinates,
            });
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        WindowQuadList quads = clipQuads(item, context);
        if (!quads.isEmpty()) {
            SurfacePixmap *pixmap = surfaceItem->pixmap();
            if (pixmap) {
                context->renderNodes.append(RenderNode{
                    .texture = bindSurfaceTexture(surfaceItem),
                    .quads = quads,
                    .transformMatrix = transformation(item, context),
                    .opacity = context->paintData.opacity(),
                    .hasAlpha = pixmap->hasAlphaChannel(),
                    .coordinateType = UnnormalizedCoordinates,
                });
            }
        }
    }

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->isVisible()) {
            createRenderNode(childItem, context);
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

void OpenGLWindow::performPaint(int mask, const QRegion &region, const WindowPaintData &data)
{
    if (region.isEmpty()) {
        return;
    }

    RenderContext renderContext {
        .paintFlags = mask,
        .clip = region,
        .paintData = data,
        .hardwareClipping = region != infiniteRegion() && (mask & Scene::PAINT_WINDOW_TRANSFORMED) && !(mask & Scene::PAINT_SCREEN_TRANSFORMED),
    };
    createRenderNode(windowItem(), &renderContext);

    int quadCount = 0;
    for (const RenderNode &node : qAsConst(renderContext.renderNodes)) {
        quadCount += node.quads.count();
    }
    if (!quadCount) {
        return;
    }

    GLShader *shader = data.shader;
    if (!shader) {
        ShaderTraits traits = ShaderTrait::MapTexture;

        if (data.opacity() != 1.0 || data.brightness() != 1.0 || data.crossFadeProgress() != 1.0)
            traits |= ShaderTrait::Modulate;

        if (data.saturation() != 1.0)
            traits |= ShaderTrait::AdjustSaturation;

        shader = ShaderManager::instance()->pushShader(traits);
    }
    shader->setUniform(GLShader::Saturation, data.saturation());

    const bool indexedQuads = GLVertexBuffer::supportsIndexedQuads();
    const GLenum primitiveType = indexedQuads ? GL_QUADS : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;
    const size_t size = verticesPerQuad * quadCount * sizeof(GLVertex2D);

    if (renderContext.hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    const GLVertexAttrib attribs[] = {
        { VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position) },
        { VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord) },
    };

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));

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

    const QMatrix4x4 modelViewProjection = modelViewProjectionMatrix(mask, data);
    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0)
            continue;

        setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);

        shader->setUniform(GLShader::ModelViewProjectionMatrix,
                           modelViewProjection * renderNode.transformMatrix);
        if (opacity != renderNode.opacity) {
            shader->setUniform(GLShader::ModulationConstant,
                               modulate(renderNode.opacity, data.brightness()));
            opacity = renderNode.opacity;
        }

        renderNode.texture->setFilter(GL_LINEAR);
        renderNode.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        renderNode.texture->bind();

        vbo->draw(region, primitiveType, renderNode.firstVertex,
                  renderNode.vertexCount, renderContext.hardwareClipping);
    }

    vbo->unbindArrays();

    setBlendEnabled(false);

    if (!data.shader)
        ShaderManager::instance()->popShader();

    if (renderContext.hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }
}

QSharedPointer<GLTexture> OpenGLWindow::windowTexture()
{
    PlatformOpenGLSurfaceTexture *frame = nullptr;
    const SurfaceItem *item = surfaceItem();

    if (item) {
        frame = static_cast<PlatformOpenGLSurfaceTexture *>(item->pixmap()->platformTexture());
    }

    if (frame && item->childItems().isEmpty() && frame->texture()) {
        return QSharedPointer<GLTexture>(new GLTexture(*frame->texture()));
    } else {
        auto effectWindow = window()->effectWindow();
        const QRect virtualGeometry = window()->bufferGeometry();
        QSharedPointer<GLTexture> texture(new GLTexture(GL_RGBA8, virtualGeometry.size() * window()->bufferScale()));

        QScopedPointer<GLRenderTarget> framebuffer(new KWin::GLRenderTarget(*texture));
        GLRenderTarget::pushRenderTarget(framebuffer.data());

        auto renderVSG = GLRenderTarget::virtualScreenGeometry();
        const QRect outputGeometry = { virtualGeometry.topLeft(), texture->size() };
        GLVertexBuffer::setVirtualScreenGeometry(outputGeometry);
        GLRenderTarget::setVirtualScreenGeometry(outputGeometry);

        QMatrix4x4 mvp;
        mvp.ortho(virtualGeometry.x(), virtualGeometry.x() + virtualGeometry.width(), virtualGeometry.y(), virtualGeometry.y() + virtualGeometry.height(), -1, 1);

        WindowPaintData data(effectWindow);
        data.setProjectionMatrix(mvp);

        performPaint(Scene::PAINT_WINDOW_TRANSFORMED, outputGeometry, data);
        GLRenderTarget::popRenderTarget();
        GLVertexBuffer::setVirtualScreenGeometry(renderVSG);
        GLRenderTarget::setVirtualScreenGeometry(renderVSG);
        return texture;
    }
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
        if (!m_unstyledTexture) {
            updateUnstyledTexture();
        }

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
    const int CS = 8;
    m_unstyledPixmap = new QPixmap(2 * CS, 2 * CS);
    m_unstyledPixmap->fill(Qt::transparent);
    QPainter p(m_unstyledPixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawEllipse(m_unstyledPixmap->rect());
    p.end();
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
    : DecorationRenderer(client)
    , m_texture()
{
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

void SceneOpenGLDecorationRenderer::render(const QRegion &region)
{
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
        painter.setWindow(QRect(geo.topLeft(), geo.size() * qPainterEffectiveDevicePixelRatio(&painter)));
        painter.setClipRect(geo);
        renderToPainter(&painter, geo);
        painter.end();

        const QRect viewportScaled(viewport.topLeft() * devicePixelRatio, viewport.size() * devicePixelRatio);
        const bool isIntegerScaling = qFuzzyCompare(devicePixelRatio, std::ceil(devicePixelRatio));
        clamp(image, isIntegerScaling ? viewportScaled : viewportScaled.marginsRemoved({1, 1, 1, 1}));

        if (rotated) {
            // TODO: get this done directly when rendering to the image
            image = rotate(image, QRect(QPoint(), rect.size()));
            viewport = QRect(viewport.y(), viewport.x(), viewport.height(), viewport.width());
        }

        const QPoint dirtyOffset = geo.topLeft() - partRect.topLeft();
        m_texture->update(image, (position + dirtyOffset - viewport.topLeft()) * image.devicePixelRatio());
    };

    const QRect geometry = region.boundingRect();

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
