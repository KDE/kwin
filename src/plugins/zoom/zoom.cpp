/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QQuickItem>

#include "zoom.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include "shortcutshintmodel.h"
#include "zoomconfig.h"

#if HAVE_ACCESSIBILITY
#include "accessibilityintegration.h"
#endif

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KStandardActions>

#include <QAction>

using namespace std::chrono_literals;

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(zoom);
}

namespace KWin
{

ZoomEffect::ZoomEffect()
    : m_shortcutsHintModel(new ShortcutsHintModel(this))
{
    ensureResources();

    ZoomConfig::instance(effects->config());
    QAction *a = nullptr;

    a = KStandardActions::zoomIn(this, &ZoomEffect::zoomIn, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    m_shortcutsHintModel->addAction(a);

    a = KStandardActions::zoomOut(this, &ZoomEffect::zoomOut, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    m_shortcutsHintModel->addAction(a);

    a = KStandardActions::actualSize(this, &ZoomEffect::actualSize, this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    m_shortcutsHintModel->addAction(a);

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
    m_shortcutsHintModel->addAction(a);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomRight"));
    a->setText(i18n("Move Zoomed Area to Right"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomRight);
    m_shortcutsHintModel->addAction(a);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomUp"));
    a->setText(i18n("Move Zoomed Area Upwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomUp);
    m_shortcutsHintModel->addAction(a);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomDown"));
    a->setText(i18n("Move Zoomed Area Downwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomDown);
    m_shortcutsHintModel->addAction(a);

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

#if HAVE_ACCESSIBILITY
    // TODO: Decide what to do about it.
    if (!effects->waylandDisplay()) {
        // on Wayland, the accessibility integration can cause KWin to hang
        m_accessibilityIntegration = new ZoomAccessibilityIntegration(this);
        connect(m_accessibilityIntegration, &ZoomAccessibilityIntegration::focusPointChanged, this, &ZoomEffect::moveFocus);
    }
#endif

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
    ZoomConfig::setInitialZoom(m_targetZoom);
    ZoomConfig::self()->save();
}

bool ZoomEffect::isFocusTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration && m_accessibilityIntegration->isFocusTrackingEnabled();
#else
    return false;
#endif
}

bool ZoomEffect::isTextCaretTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration && m_accessibilityIntegration->isTextCaretTrackingEnabled();
#else
    return false;
#endif
}

GLTexture *ZoomEffect::ensureCursorTexture()
{
    if (!m_cursorTexture || m_cursorTextureDirty) {
        m_cursorTexture.reset();
        m_cursorTextureDirty = false;
        const auto cursor = effects->cursorImage();
        if (!cursor.image().isNull()) {
            m_cursorTexture = GLTexture::upload(cursor.image());
            if (!m_cursorTexture) {
                return nullptr;
            }
            m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        }
    }
    return m_cursorTexture.get();
}

void ZoomEffect::markCursorTextureDirty()
{
    m_cursorTextureDirty = true;
}

void ZoomEffect::showCursor()
{
    if (m_isMouseHidden) {
        disconnect(effects, &EffectsHandler::cursorShapeChanged, this, &ZoomEffect::markCursorTextureDirty);
        // show the previously hidden mouse-pointer again and free the loaded texture/picture.
        effects->showCursor();
        m_cursorTexture.reset();
        m_isMouseHidden = false;
    }
}

void ZoomEffect::hideCursor()
{
    if (m_mouseTracking == MouseTrackingProportional && m_mousePointer == MousePointerKeep) {
        return; // don't replace the actual cursor by a static image for no reason.
    }
    if (!m_isMouseHidden) {
        // try to load the cursor-theme into a OpenGL texture and if successful then hide the mouse-pointer
        GLTexture *texture = nullptr;
        if (effects->isOpenGLCompositing()) {
            texture = ensureCursorTexture();
        }
        if (texture) {
            effects->hideCursor();
            connect(effects, &EffectsHandler::cursorShapeChanged, this, &ZoomEffect::markCursorTextureDirty);
            m_isMouseHidden = true;
        }
    }
}

static Qt::KeyboardModifiers stringToKeyboardModifiers(const QString &string)
{
    const QStringList parts = string.split(QLatin1Char('+'));
    if (parts.isEmpty()) {
        return Qt::KeyboardModifiers();
    }

    Qt::KeyboardModifiers modifiers;
    for (const QString &part : parts) {
        if (part == QLatin1String("Meta")) {
            modifiers |= Qt::MetaModifier;
        } else if (part == QLatin1String("Ctrl") || part == QLatin1String("Control")) {
            // NOTE: "Meta+Control" is provided KQuickControls.KeySequenceItem instead of "Meta+Ctrl"
            modifiers |= Qt::ControlModifier;
        } else if (part == QLatin1String("Alt")) {
            modifiers |= Qt::AltModifier;
        } else if (part == QLatin1String("Shift")) {
            modifiers |= Qt::ShiftModifier;
        }
    }

    return modifiers;
}

void ZoomEffect::reconfigure(ReconfigureFlags)
{
    ZoomConfig::self()->read();

    // On zoom-in and zoom-out change the zoom by the defined zoom-factor.
    m_zoomFactor = std::max(0.1, ZoomConfig::zoomFactor());
    Q_EMIT zoomFactorChanged();

    m_pixelGridZoom = ZoomConfig::pixelGridZoom();
    Q_EMIT pixelGridZoomChanged();

    // Visibility of the mouse-pointer.
    m_mousePointer = MousePointerType(ZoomConfig::mousePointer());

    // Track moving of the mouse.
    m_mouseTracking = MouseTrackingType(ZoomConfig::mouseTracking());

#if HAVE_ACCESSIBILITY
    if (m_accessibilityIntegration) {
        // Enable tracking of the focused location.
        m_accessibilityIntegration->setFocusTrackingEnabled(ZoomConfig::enableFocusTracking());

        // Enable tracking of the text caret.
        m_accessibilityIntegration->setTextCaretTrackingEnabled(ZoomConfig::enableTextCaretTracking());
    }
#endif

    // The time in milliseconds to wait before a focus-event takes away a mouse-move.
    m_focusDelay = std::max(uint(0), ZoomConfig::focusDelay());

    // The factor the zoom-area will be moved on touching an edge on push-mode or using the navigation KAction's.
    m_moveFactor = std::max(0.1, ZoomConfig::moveFactor());

    const Qt::KeyboardModifiers pointerAxisModifiers = stringToKeyboardModifiers(ZoomConfig::pointerAxisGestureModifiers());
    m_shortcutsHintModel->setPointerAxisGestureModifiers(pointerAxisModifiers);
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

    // Enable the hint overlay shown during zoom
    m_enableHintOverlay = ZoomConfig::enableHintOverlay();
    if (m_enableHintOverlay && !m_overlay) {
        m_overlay = std::make_unique<OffscreenQuickScene>();
        const auto url = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin-wayland/effects/zoom/qml/overlay.qml")));
        m_overlay->setSource(url, {{QStringLiteral("effect"), QVariant::fromValue(this)}});
        connect(m_overlay.get(), &OffscreenQuickScene::repaintNeeded, this, []() {
            effects->addRepaintFull();
        });
    } else if (!m_enableHintOverlay && m_overlay) {
        m_overlay.reset();
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
        showCursor();
    } else {
        hideCursor();
    }

    effects->prePaintScreen(data, presentTime);
}

ZoomEffect::OffscreenData *ZoomEffect::ensureOffscreenData(const RenderTarget &renderTarget, const RenderViewport &viewport, Output *screen)
{
    const QSize nativeSize = renderTarget.size();

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

void ZoomEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    OffscreenData *offscreenData = ensureOffscreenData(renderTarget, viewport, screen);
    if (!offscreenData) {
        return;
    }

    // Render the scene in an offscreen texture and then upscale it.
    RenderTarget offscreenRenderTarget(offscreenData->framebuffer.get(), renderTarget.colorDescription());
    RenderViewport offscreenViewport(viewport.renderRect(), viewport.scale(), offscreenRenderTarget);
    GLFramebuffer::pushFramebuffer(offscreenData->framebuffer.get());
    effects->paintScreen(offscreenRenderTarget, offscreenViewport, mask, region, screen);
    GLFramebuffer::popFramebuffer();

    const QSize screenSize = effects->virtualScreenSize();
    const auto scale = viewport.scale();

    // mouse-tracking allows navigation of the zoom-area using the mouse.
    qreal xTranslation = 0;
    qreal yTranslation = 0;
    switch (m_mouseTracking) {
    case MouseTrackingProportional:
        xTranslation = -int(m_cursorPoint.x() * (m_zoom - 1.0));
        yTranslation = -int(m_cursorPoint.y() * (m_zoom - 1.0));
        m_prevPoint = m_cursorPoint;
        break;
    case MouseTrackingCentered:
        m_prevPoint = m_cursorPoint;
        // fall through
    case MouseTrackingDisabled:
        xTranslation = std::min(0, std::max(int(screenSize.width() - screenSize.width() * m_zoom), int(screenSize.width() / 2 - m_prevPoint.x() * m_zoom)));
        yTranslation = std::min(0, std::max(int(screenSize.height() - screenSize.height() * m_zoom), int(screenSize.height() / 2 - m_prevPoint.y() * m_zoom)));
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
        xTranslation = -int(m_prevPoint.x() * (m_zoom - 1.0));
        yTranslation = -int(m_prevPoint.y() * (m_zoom - 1.0));
        break;
    }
    }

    // use the focusPoint if focus tracking is enabled
    if (isFocusTrackingEnabled() || isTextCaretTrackingEnabled()) {
        bool acceptFocus = true;
        if (m_mouseTracking != MouseTrackingDisabled && m_focusDelay > 0) {
            // Wait some time for the mouse before doing the switch. This serves as threshold
            // to prevent the focus from jumping around to much while working with the mouse.
            const int msecs = m_lastMouseEvent.msecsTo(m_lastFocusEvent);
            acceptFocus = msecs > m_focusDelay;
        }
        if (acceptFocus) {
            xTranslation = -int(m_focusPoint.x() * (m_zoom - 1.0));
            yTranslation = -int(m_focusPoint.y() * (m_zoom - 1.0));
            m_prevPoint = m_focusPoint;
        }
    }

    // Render transformed offscreen texture.
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLShader *shader = shaderForZoom(m_zoom);
    ShaderManager::instance()->pushShader(shader);
    for (auto &[screen, offscreen] : m_offscreenData) {
        QMatrix4x4 matrix;
        matrix.translate(xTranslation * scale, yTranslation * scale);
        matrix.scale(m_zoom, m_zoom);
        matrix.translate(offscreen.viewport.x() * scale, offscreen.viewport.y() * scale);

        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix() * matrix);
        shader->setUniform(GLShader::IntUniform::TextureWidth, offscreen.texture->width());
        shader->setUniform(GLShader::IntUniform::TextureHeight, offscreen.texture->height());
        shader->setColorspaceUniforms(offscreen.color, renderTarget.colorDescription(), RenderingIntent::Perceptual);

        offscreen.texture->render(offscreen.viewport.size() * scale);
    }
    ShaderManager::instance()->popShader();

    if (m_mousePointer != MousePointerHide) {
        // Draw the mouse-texture at the position matching to zoomed-in image of the desktop. Hiding the
        // previous mouse-cursor and drawing our own fake mouse-cursor is needed to be able to scale the
        // mouse-cursor up and to re-position those mouse-cursor to match to the chosen zoom-level.

        GLTexture *cursorTexture = ensureCursorTexture();
        if (cursorTexture) {
            const auto cursor = effects->cursorImage();
            QSizeF cursorSize = QSizeF(cursor.image().size()) / cursor.image().devicePixelRatio();
            if (m_mousePointer == MousePointerScale) {
                cursorSize *= m_zoom;
            }

            const QPointF p = (effects->cursorPos() - cursor.hotSpot()) * m_zoom + QPoint(xTranslation, yTranslation);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
            s->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
            QMatrix4x4 mvp = viewport.projectionMatrix();
            mvp.translate(p.x() * scale, p.y() * scale);
            s->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
            cursorTexture->render(cursorSize * scale);
            ShaderManager::instance()->popShader();
            glDisable(GL_BLEND);
        }
    }

    if (m_enableHintOverlay && m_overlay && m_overlay->rootItem()) {
        // TODO: A check is likely needed here to avoid 'effect: OffscreenQuickView has no fbo!'
        // TODO: RTL?
        const qreal overlayMargins = m_overlay->rootItem()->property("margins").toReal();
        m_overlay->setGeometry(QRect(overlayMargins, overlayMargins, m_overlay->rootItem()->implicitWidth(), m_overlay->rootItem()->implicitHeight()));
        effects->renderOffscreenQuickView(renderTarget, viewport, m_overlay.get());
    }
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

void ZoomEffect::slotScreenRemoved(Output *screen)
{
    if (auto it = m_offscreenData.find(screen); it != m_offscreenData.end()) {
        effects->makeOpenGLContextCurrent();
        m_offscreenData.erase(it);
    }
}

void ZoomEffect::moveFocus(const QPoint &point)
{
    if (m_zoom == 1.0) {
        return;
    }
    m_focusPoint = point;
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

double ZoomEffect::pixelGridZoom() const
{
    return m_pixelGridZoom;
}

ShortcutsHintModel *ZoomEffect::shortcutsHintModel() const
{
    return m_shortcutsHintModel;
}

bool ZoomEffect::screenExistsAt(const QPoint &point) const
{
    const Output *output = effects->screenAt(point);
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
        connect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
        m_cursorPoint = effects->cursorPos().toPoint();
    } else if (!newActive && oldActive) {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
    }
    m_targetZoom = value;
    Q_EMIT targetZoomChanged();
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

} // namespace

#include "moc_zoom.cpp"
