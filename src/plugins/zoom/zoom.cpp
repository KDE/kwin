/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "zoom.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "focustracker.h"
#include "opengl/glutils.h"
#include "scene/cursoritem.h"
#include "scene/workspacescene.h"
#include "textcarettracker.h"
#include "utils/keys.h"
#include "zoomconfig.h"

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KStandardActions>

#include <QAction>
#include <QTimer>

using namespace std::chrono_literals;

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(zoom);
}

namespace KWin
{

ZoomEffect::ZoomEffect()
{
    ensureResources();

    m_configurationTimer = std::make_unique<QTimer>();
    m_configurationTimer->setInterval(1s);
    m_configurationTimer->setSingleShot(true);
    connect(m_configurationTimer.get(), &QTimer::timeout, this, &ZoomEffect::saveInitialZoom);

    ZoomConfig::instance(effects->config());
    QAction *a = nullptr;
    a = KStandardActions::zoomIn(this, &ZoomEffect::zoomIn, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));

    a = KStandardActions::zoomOut(this, &ZoomEffect::zoomOut, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));

    a = KStandardActions::actualSize(this, &ZoomEffect::actualSize, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));

    m_touchpadAction = std::make_unique<QAction>();
    connect(m_touchpadAction.get(), &QAction::triggered, this, [this]() {
        const double threshold = 1.15;
        if (m_targetZoom < threshold) {
            zoomTo(1.0);
        }
        m_lastPinchProgress = 0;
    });
    effects->registerTouchpadPinchShortcut(PinchDirection::Expanding, 3, m_touchpadAction.get(), [this](qreal progress) {
        const qreal delta = progress - m_lastPinchProgress;
        m_lastPinchProgress = progress;
        realtimeZoom(delta);
    });
    effects->registerTouchpadPinchShortcut(PinchDirection::Contracting, 3, m_touchpadAction.get(), [this](qreal progress) {
        const qreal delta = progress - m_lastPinchProgress;
        m_lastPinchProgress = progress;
        realtimeZoom(-delta);
    });

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomLeft"));
    a->setText(i18n("Move Zoomed Area to Left"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomLeft);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomRight"));
    a->setText(i18n("Move Zoomed Area to Right"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomRight);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomUp"));
    a->setText(i18n("Move Zoomed Area Upwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomUp);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomDown"));
    a->setText(i18n("Move Zoomed Area Downwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomDown);

    // TODO: these two actions don't belong into the effect. They need to be moved into KWin core
    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToFocus"));
    a->setText(i18n("Move Mouse to Focus"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F5));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F5));
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToFocus);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToCenter"));
    a->setText(i18n("Move Mouse to Center"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F6));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F6));
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToCenter);

    m_timeline.setDuration(350);
    m_timeline.setFrameRange(0, 100);
    connect(&m_timeline, &QTimeLine::frameChanged, this, &ZoomEffect::timelineFrameChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &ZoomEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &ZoomEffect::slotScreenRemoved);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        slotWindowAdded(w);
    }

    reconfigure(ReconfigureAll);

    const double initialZoom = ZoomConfig::initialZoom();
    if (initialZoom > 1.0) {
        zoomTo(initialZoom);
    }
}

ZoomEffect::~ZoomEffect()
{
    // switch off and free resources
    showCursor();
    // Save the zoom value.
    saveInitialZoom();
}

QPointF ZoomEffect::calculateCursorItemPosition() const
{
    return Cursors::self()->mouse()->pos() * m_zoom + QPoint(m_xTranslation, m_yTranslation);
}

void ZoomEffect::showCursor()
{
    if (m_cursorHidden) {
        m_cursorItem.reset();
        m_cursorHidden = false;
        effects->showCursor();
    }
}

void ZoomEffect::hideCursor()
{
    if (m_mouseTracking == MouseTrackingProportional && m_mousePointer == MousePointerKeep) {
        return; // don't replace the actual cursor by a static image for no reason.
    }
    if (!m_cursorHidden) {
        effects->hideCursor();
        m_cursorHidden = true;

        if (m_mousePointer == MousePointerKeep || m_mousePointer == MousePointerScale) {
            m_cursorItem = std::make_unique<CursorItem>(effects->scene()->overlayItem());
            m_cursorItem->setPosition(calculateCursorItemPosition());
            connect(Cursors::self()->mouse(), &Cursor::posChanged, m_cursorItem.get(), [this]() {
                m_cursorItem->setPosition(calculateCursorItemPosition());
            });
        }
    }
}

void ZoomEffect::reconfigure(ReconfigureFlags)
{
    ZoomConfig::self()->read();
    // On zoom-in and zoom-out change the zoom by the defined zoom-factor.
    m_zoomFactor = std::max(0.1, ZoomConfig::zoomFactor());
    m_pixelGridZoom = ZoomConfig::pixelGridZoom();
    // Visibility of the mouse-pointer.
    m_mousePointer = MousePointerType(ZoomConfig::mousePointer());
    // Track moving of the mouse.
    m_mouseTracking = MouseTrackingType(ZoomConfig::mouseTracking());

    if (ZoomConfig::enableFocusTracking()) {
        if (m_targetZoom > 1) {
            trackFocus();
        }
    } else {
#if KWIN_BUILD_QACCESSIBILITYCLIENT
        m_focusTracker.reset();
#endif
    }

    if (ZoomConfig::enableTextCaretTracking()) {
        if (m_targetZoom > 1) {
            trackTextCaret();
        }
    } else {
        m_textCaretTracker.reset();
    }

    if (!ZoomConfig::enableFocusTracking() && !ZoomConfig::enableTextCaretTracking()) {
        m_focusPoint.reset();
    }

    // The time in milliseconds to wait before a focus-event takes away a mouse-move.
    m_focusDelay = std::max(uint(0), ZoomConfig::focusDelay());
    // The factor the zoom-area will be moved on touching an edge on push-mode or using the navigation KAction's.
    m_moveFactor = std::max(0.1, ZoomConfig::moveFactor());

    const Qt::KeyboardModifiers pointerAxisModifiers = stringToKeyboardModifiers(ZoomConfig::pointerAxisGestureModifiers());
    if (m_axisModifiers != pointerAxisModifiers) {
        m_zoomInAxisAction.reset();
        m_zoomOutAxisAction.reset();
        m_axisModifiers = pointerAxisModifiers;

        if (pointerAxisModifiers) {
            m_zoomInAxisAction = std::make_unique<QAction>();
            connect(m_zoomInAxisAction.get(), &QAction::triggered, this, &ZoomEffect::zoomIn);
            effects->registerAxisShortcut(pointerAxisModifiers, PointerAxisUp, m_zoomInAxisAction.get());

            m_zoomOutAxisAction = std::make_unique<QAction>();
            connect(m_zoomOutAxisAction.get(), &QAction::triggered, this, &ZoomEffect::zoomOut);
            effects->registerAxisShortcut(pointerAxisModifiers, PointerAxisDown, m_zoomOutAxisAction.get());
        }
    }
}

void ZoomEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    data.mask |= PAINT_SCREEN_TRANSFORMED;
    if (m_zoom != m_targetZoom) {
        int time = 0;
        if (m_lastPresentTime.count()) {
            time = (presentTime - m_lastPresentTime).count();
        }
        m_lastPresentTime = presentTime;

        const float zoomDist = std::abs(m_targetZoom - m_sourceZoom);
        if (m_targetZoom > m_zoom) {
            m_zoom = std::min(m_zoom + ((zoomDist * time) / animationTime(std::chrono::milliseconds(int(150 * m_zoomFactor)))), m_targetZoom);
        } else {
            m_zoom = std::max(m_zoom - ((zoomDist * time) / animationTime(std::chrono::milliseconds(int(150 * m_zoomFactor)))), m_targetZoom);
        }
    }

    if (m_zoom == 1.0) {
        m_focusPoint.reset();

        showCursor();
    } else {
        hideCursor();
        if (m_mousePointer == MousePointerScale) {
            m_cursorItem->setTransform(QTransform::fromScale(m_zoom, m_zoom));
        }
    }

    const QSize screenSize = effects->virtualScreenSize();

    // mouse-tracking allows navigation of the zoom-area using the mouse.
    switch (m_mouseTracking) {
    case MouseTrackingProportional:
        m_xTranslation = -int(m_cursorPoint.x() * (m_zoom - 1.0));
        m_yTranslation = -int(m_cursorPoint.y() * (m_zoom - 1.0));
        m_prevPoint = m_cursorPoint;
        break;
    case MouseTrackingCentered:
        m_prevPoint = m_cursorPoint;
        // fall through
    case MouseTrackingDisabled:
        m_xTranslation = std::min(0, std::max(int(screenSize.width() - screenSize.width() * m_zoom), int(screenSize.width() / 2 - m_prevPoint.x() * m_zoom)));
        m_yTranslation = std::min(0, std::max(int(screenSize.height() - screenSize.height() * m_zoom), int(screenSize.height() / 2 - m_prevPoint.y() * m_zoom)));
        break;
    case MouseTrackingPush: {
        // touching an edge of the screen moves the zoom-area in that direction.
        const int x = m_cursorPoint.x() * m_zoom - m_prevPoint.x() * (m_zoom - 1.0);
        const int y = m_cursorPoint.y() * m_zoom - m_prevPoint.y() * (m_zoom - 1.0);
        const int threshold = 4;
        const QRectF currScreen = effects->screenAt(QPoint(x, y))->geometry();

        // bounds of the screen the cursor's on
        const int screenTop = currScreen.top();
        const int screenLeft = currScreen.left();
        const int screenRight = currScreen.right();
        const int screenBottom = currScreen.bottom();
        const int screenCenterX = currScreen.center().x();
        const int screenCenterY = currScreen.center().y();

        // figure out whether we have adjacent displays in all 4 directions
        // We pan within the screen in directions where there are no adjacent screens.
        const bool adjacentLeft = screenExistsAt(QPoint(screenLeft - 1, screenCenterY));
        const bool adjacentRight = screenExistsAt(QPoint(screenRight + 1, screenCenterY));
        const bool adjacentTop = screenExistsAt(QPoint(screenCenterX, screenTop - 1));
        const bool adjacentBottom = screenExistsAt(QPoint(screenCenterX, screenBottom + 1));

        m_xMove = m_yMove = 0;
        if (x < screenLeft + threshold && !adjacentLeft) {
            m_xMove = (x - threshold - screenLeft) / m_zoom;
        } else if (x > screenRight - threshold && !adjacentRight) {
            m_xMove = (x + threshold - screenRight) / m_zoom;
        }
        if (y < screenTop + threshold && !adjacentTop) {
            m_yMove = (y - threshold - screenTop) / m_zoom;
        } else if (y > screenBottom - threshold && !adjacentBottom) {
            m_yMove = (y + threshold - screenBottom) / m_zoom;
        }
        if (m_xMove) {
            m_prevPoint.setX(m_prevPoint.x() + m_xMove);
        }
        if (m_yMove) {
            m_prevPoint.setY(m_prevPoint.y() + m_yMove);
        }
        m_xTranslation = -int(m_prevPoint.x() * (m_zoom - 1.0));
        m_yTranslation = -int(m_prevPoint.y() * (m_zoom - 1.0));
        break;
    }
    }

    // use the focusPoint if focus tracking is enabled
    if (m_focusPoint) {
        bool acceptFocus = true;
        if (m_mouseTracking != MouseTrackingDisabled && m_focusDelay > 0) {
            // Wait some time for the mouse before doing the switch. This serves as threshold
            // to prevent the focus from jumping around to much while working with the mouse.
            acceptFocus = m_lastMouseEvent.isNull() || m_lastMouseEvent.msecsTo(m_lastFocusEvent) > m_focusDelay;
        }
        if (acceptFocus) {
            m_xTranslation = -int(m_focusPoint->x() * (m_zoom - 1.0));
            m_yTranslation = -int(m_focusPoint->y() * (m_zoom - 1.0));
            m_prevPoint = *m_focusPoint;
        }
    }

    if (m_cursorItem) {
        // x and y translation are changed, update the cursor position
        m_cursorItem->setPosition(calculateCursorItemPosition());
    }

    effects->prePaintScreen(data, presentTime);
}

ZoomEffect::OffscreenData *ZoomEffect::ensureOffscreenData(const RenderTarget &renderTarget, const RenderViewport &viewport, LogicalOutput *screen)
{
    const QSize nativeSize = viewport.deviceSize();

    // TODO this should be per view, rather than per logical screen.
    OffscreenData &data = m_offscreenData[screen];
    data.viewport = viewport.renderRect();
    data.color = renderTarget.colorDescription();

    const GLenum textureFormat = renderTarget.colorDescription() == ColorDescription::sRGB ? GL_RGBA8 : GL_RGBA16F;
    if (!data.texture || data.texture->size() != nativeSize || data.texture->internalFormat() != textureFormat) {
        data.texture = GLTexture::allocate(textureFormat, nativeSize);
        if (!data.texture) {
            return nullptr;
        }
        data.texture->setFilter(GL_LINEAR);
        data.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        data.framebuffer = std::make_unique<GLFramebuffer>(data.texture.get());
    }

    data.texture->setContentTransform(renderTarget.transform());
    return &data;
}

GLShader *ZoomEffect::shaderForZoom(double zoom)
{
    if (zoom < m_pixelGridZoom) {
        return ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
    } else {
        if (!m_pixelGridShader) {
            m_pixelGridShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QStringLiteral(":/effects/zoom/shaders/pixelgrid.frag"));
        }
        return m_pixelGridShader.get();
    }
}

void ZoomEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion, LogicalOutput *screen)
{
    OffscreenData *offscreenData = ensureOffscreenData(renderTarget, viewport, screen);
    if (!offscreenData) {
        return;
    }

    // Render the scene in an offscreen texture and then upscale it.
    RenderTarget offscreenRenderTarget(offscreenData->framebuffer.get(), renderTarget.colorDescription());
    RenderViewport offscreenViewport(viewport.renderRect(), viewport.scale(), offscreenRenderTarget, QPoint());
    GLFramebuffer::pushFramebuffer(offscreenData->framebuffer.get());
    effects->paintScreen(offscreenRenderTarget, offscreenViewport, mask, deviceRegion, screen);
    GLFramebuffer::popFramebuffer();

    const auto scale = viewport.scale();

    // Render transformed offscreen texture.
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLShader *shader = shaderForZoom(m_zoom);
    ShaderManager::instance()->pushShader(shader);
    for (auto &[screen, offscreen] : m_offscreenData) {
        QMatrix4x4 matrix;
        matrix.translate(m_xTranslation * scale, m_yTranslation * scale);
        matrix.scale(m_zoom, m_zoom);
        matrix.translate(offscreen.viewport.x() * scale, offscreen.viewport.y() * scale);

        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix() * matrix);
        shader->setUniform(GLShader::IntUniform::TextureWidth, offscreen.texture->width());
        shader->setUniform(GLShader::IntUniform::TextureHeight, offscreen.texture->height());
        shader->setColorspaceUniforms(offscreen.color, renderTarget.colorDescription(), RenderingIntent::Perceptual);

        offscreen.texture->render(offscreen.viewport.size() * scale);
    }
    ShaderManager::instance()->popShader();
}

void ZoomEffect::postPaintScreen()
{
    if (m_zoom == m_targetZoom) {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    }

    if (m_zoom == 1.0 || m_zoom != m_targetZoom) {
        // Either animation is running or the zoom effect has stopped.
        effects->addRepaintFull();
    }

    effects->postPaintScreen();
}

void ZoomEffect::zoomIn()
{
    zoomTo(-1.0);
}

void ZoomEffect::zoomTo(double to)
{
    m_sourceZoom = m_zoom;
    if (to < 0.0) {
        setTargetZoom(m_targetZoom * m_zoomFactor);
    } else {
        setTargetZoom(to);
    }
    m_cursorPoint = effects->cursorPos().toPoint();
    if (m_mouseTracking == MouseTrackingDisabled) {
        m_prevPoint = m_cursorPoint;
    }
}

void ZoomEffect::zoomOut()
{
    m_sourceZoom = m_zoom;
    setTargetZoom(m_targetZoom / m_zoomFactor);
    if ((m_zoomFactor > 1 && m_targetZoom < 1.01) || (m_zoomFactor < 1 && m_targetZoom > 0.99)) {
        setTargetZoom(1);
    }
    if (m_mouseTracking == MouseTrackingDisabled) {
        m_prevPoint = effects->cursorPos().toPoint();
    }
}

void ZoomEffect::actualSize()
{
    m_sourceZoom = m_zoom;
    setTargetZoom(1);
}

void ZoomEffect::timelineFrameChanged(int /* frame */)
{
    const QSize screenSize = effects->virtualScreenSize();
    m_prevPoint.setX(std::max(0, std::min(screenSize.width(), m_prevPoint.x() + m_xMove)));
    m_prevPoint.setY(std::max(0, std::min(screenSize.height(), m_prevPoint.y() + m_yMove)));
    m_cursorPoint = m_prevPoint;
    effects->addRepaintFull();
}

void ZoomEffect::moveZoom(int x, int y)
{
    if (m_timeline.state() == QTimeLine::Running) {
        m_timeline.stop();
    }

    const QSize screenSize = effects->virtualScreenSize();
    if (x < 0) {
        m_xMove = -std::max(1.0, screenSize.width() / m_zoom / m_moveFactor);
    } else if (x > 0) {
        m_xMove = std::max(1.0, screenSize.width() / m_zoom / m_moveFactor);
    } else {
        m_xMove = 0;
    }

    if (y < 0) {
        m_yMove = -std::max(1.0, screenSize.height() / m_zoom / m_moveFactor);
    } else if (y > 0) {
        m_yMove = std::max(1.0, screenSize.height() / m_zoom / m_moveFactor);
    } else {
        m_yMove = 0;
    }

    m_timeline.start();
}

void ZoomEffect::moveZoomLeft()
{
    moveZoom(-1, 0);
}

void ZoomEffect::moveZoomRight()
{
    moveZoom(1, 0);
}

void ZoomEffect::moveZoomUp()
{
    moveZoom(0, -1);
}

void ZoomEffect::moveZoomDown()
{
    moveZoom(0, 1);
}

void ZoomEffect::moveMouseToFocus()
{
    const auto window = effects->activeWindow();
    if (!window) {
        return;
    }
    const auto center = window->frameGeometry().center();
    QCursor::setPos(center.x(), center.y());
}

void ZoomEffect::moveMouseToCenter()
{
    const QRect r = effects->activeScreen()->geometry();
    QCursor::setPos(r.x() + r.width() / 2, r.y() + r.height() / 2);
}

void ZoomEffect::slotMouseChanged(const QPointF &pos, const QPointF &old)
{
    if (m_zoom == 1.0) {
        return;
    }
    m_cursorPoint = pos.toPoint();
    if (pos != old) {
        m_lastMouseEvent = QTime::currentTime();
        effects->addRepaintFull();
    }
}

void ZoomEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowDamaged, this, &ZoomEffect::slotWindowDamaged);
}

void ZoomEffect::slotWindowDamaged()
{
    if (m_zoom != 1.0) {
        effects->addRepaintFull();
    }
}

void ZoomEffect::slotScreenRemoved(LogicalOutput *screen)
{
    if (auto it = m_offscreenData.find(screen); it != m_offscreenData.end()) {
        effects->makeOpenGLContextCurrent();
        m_offscreenData.erase(it);
    }
}

void ZoomEffect::moveFocus(const QPointF &point)
{
    if (m_zoom == 1.0) {
        return;
    }
    m_focusPoint = point.toPoint();
    m_lastFocusEvent = QTime::currentTime();
    effects->addRepaintFull();
}

bool ZoomEffect::isActive() const
{
    return m_zoom != 1.0 || m_zoom != m_targetZoom;
}

int ZoomEffect::requestedEffectChainPosition() const
{
    return 10;
}

qreal ZoomEffect::configuredZoomFactor() const
{
    return m_zoomFactor;
}

int ZoomEffect::configuredMousePointer() const
{
    return m_mousePointer;
}

int ZoomEffect::configuredMouseTracking() const
{
    return m_mouseTracking;
}

int ZoomEffect::configuredFocusDelay() const
{
    return m_focusDelay;
}

qreal ZoomEffect::configuredMoveFactor() const
{
    return m_moveFactor;
}

qreal ZoomEffect::targetZoom() const
{
    return m_targetZoom;
}

void ZoomEffect::saveInitialZoom()
{
    ZoomConfig::setInitialZoom(m_targetZoom);
    ZoomConfig::self()->save();
}

bool ZoomEffect::screenExistsAt(const QPoint &point) const
{
    const LogicalOutput *output = effects->screenAt(point);
    return output && output->geometry().contains(point);
}

void ZoomEffect::setTargetZoom(double value)
{
    value = std::clamp(value, 1.0, 100.0);
    if (m_targetZoom == value) {
        return;
    }
    const bool newActive = value != 1.0;
    const bool oldActive = m_targetZoom != 1.0;
    if (newActive && !oldActive) {
        if (ZoomConfig::enableTextCaretTracking()) {
            trackTextCaret();
        }
        if (ZoomConfig::enableFocusTracking()) {
            trackFocus();
        }

        connect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
        m_cursorPoint = effects->cursorPos().toPoint();
    } else if (!newActive && oldActive) {
        m_textCaretTracker.reset();
#if KWIN_BUILD_QACCESSIBILITYCLIENT
        m_focusTracker.reset();
#endif
        disconnect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
    }
    m_targetZoom = value;
    m_configurationTimer->start();
    effects->addRepaintFull();
}

void ZoomEffect::realtimeZoom(double delta)
{
    // for the change speed to feel roughly linear,
    // we have to increase the delta at higher zoom levels
    delta *= m_targetZoom / 2;
    setTargetZoom(m_targetZoom + delta);
    // skip the animation, we want this to be real time
    m_zoom = m_targetZoom;
    if (m_zoom == 1.0) {
        showCursor();
    }
}

void ZoomEffect::trackTextCaret()
{
    m_textCaretTracker = std::make_unique<TextCaretTracker>();
    connect(m_textCaretTracker.get(), &TextCaretTracker::moved, this, &ZoomEffect::moveFocus);
}

void ZoomEffect::trackFocus()
{
#if KWIN_BUILD_QACCESSIBILITYCLIENT
    // Dbus-based focus tracking is disabled on wayland because libqaccessibilityclient has
    // blocking dbus calls, which can result in kwin_wayland lockups.

    static bool forceFocusTracking = qEnvironmentVariableIntValue("KWIN_WAYLAND_ZOOM_FORCE_LEGACY_FOCUS_TRACKING");
    if (!forceFocusTracking) {
        return;
    }

    m_focusTracker = std::make_unique<FocusTracker>();
    connect(m_focusTracker.get(), &FocusTracker::moved, this, &ZoomEffect::moveFocus);
#endif
}

} // namespace

#include "moc_zoom.cpp"
