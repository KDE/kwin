/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "zoom.h"
// KConfigSkeleton
#include "zoomconfig.h"

#if HAVE_ACCESSIBILITY
#include "accessibilityintegration.h"
#endif

#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QVector2D>
#include <kstandardaction.h>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <kwinglutils.h>

namespace KWin
{

ZoomEffect::ZoomEffect()
    : Effect()
    , zoom(1)
    , target_zoom(1)
    , polling(false)
    , zoomFactor(1.25)
    , mouseTracking(MouseTrackingProportional)
    , mousePointer(MousePointerScale)
    , focusDelay(350)   // in milliseconds
    , isMouseHidden(false)
    , xMove(0)
    , yMove(0)
    , moveFactor(20.0)
    , lastPresentTime(std::chrono::milliseconds::zero())
{
    initConfig<ZoomConfig>();
    QAction* a = nullptr;
    a = KStandardAction::zoomIn(this, SLOT(zoomIn()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Equal, a);
    effects->registerAxisShortcut(Qt::ControlModifier | Qt::MetaModifier, PointerAxisDown, a);

    a = KStandardAction::zoomOut(this, SLOT(zoomOut()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Minus, a);
    effects->registerAxisShortcut(Qt::ControlModifier | Qt::MetaModifier, PointerAxisUp, a);

    a = KStandardAction::actualSize(this, SLOT(actualSize()), this);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_0, a);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomLeft"));
    a->setText(i18n("Move Zoomed Area to Left"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomLeft);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomRight"));
    a->setText(i18n("Move Zoomed Area to Right"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomRight);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomUp"));
    a->setText(i18n("Move Zoomed Area Upwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomUp);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomDown"));
    a->setText(i18n("Move Zoomed Area Downwards"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());
    effects->registerGlobalShortcut(QKeySequence(), a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomDown);

    // TODO: these two actions don't belong into the effect. They need to be moved into KWin core
    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToFocus"));
    a->setText(i18n("Move Mouse to Focus"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F5);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F5);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_F5, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToFocus);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToCenter"));
    a->setText(i18n("Move Mouse to Center"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F6);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F6);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_F6, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToCenter);

    timeline.setDuration(350);
    timeline.setFrameRange(0, 100);
    connect(&timeline, &QTimeLine::frameChanged, this, &ZoomEffect::timelineFrameChanged);
    connect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowDamaged, this, &ZoomEffect::slotWindowDamaged);

#if HAVE_ACCESSIBILITY
    m_accessibilityIntegration = new ZoomAccessibilityIntegration(this);
    connect(m_accessibilityIntegration, &ZoomAccessibilityIntegration::focusPointChanged, this, &ZoomEffect::moveFocus);
#endif

    source_zoom = -1; // used to trigger initialZoom reading
    reconfigure(ReconfigureAll);
}

ZoomEffect::~ZoomEffect()
{
    // switch off and free resources
    showCursor();
    // Save the zoom value.
    ZoomConfig::setInitialZoom(target_zoom);
    ZoomConfig::self()->save();
}

bool ZoomEffect::isFocusTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration->isFocusTrackingEnabled();
#else
    return false;
#endif
}

bool ZoomEffect::isTextCaretTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration->isTextCaretTrackingEnabled();
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
            m_cursorTexture.reset(new GLTexture(cursor.image()));
            m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        }
    }
    return m_cursorTexture.data();
}

void ZoomEffect::markCursorTextureDirty()
{
    m_cursorTextureDirty = true;
}

void ZoomEffect::showCursor()
{
    if (isMouseHidden) {
        disconnect(effects, &EffectsHandler::cursorShapeChanged, this, &ZoomEffect::markCursorTextureDirty);
        // show the previously hidden mouse-pointer again and free the loaded texture/picture.
        effects->showCursor();
        m_cursorTexture.reset();
        isMouseHidden = false;
    }
}

void ZoomEffect::hideCursor()
{
    if (mouseTracking == MouseTrackingProportional && mousePointer == MousePointerKeep)
        return; // don't replace the actual cursor by a static image for no reason.
    if (!isMouseHidden) {
        // try to load the cursor-theme into a OpenGL texture and if successful then hide the mouse-pointer
        GLTexture *texture = nullptr;
        if (effects->isOpenGLCompositing()) {
            texture = ensureCursorTexture();
        }
        if (texture) {
            effects->hideCursor();
            connect(effects, &EffectsHandler::cursorShapeChanged, this, &ZoomEffect::markCursorTextureDirty);
            isMouseHidden = true;
        }
    }
}

void ZoomEffect::reconfigure(ReconfigureFlags)
{
    ZoomConfig::self()->read();
    // On zoom-in and zoom-out change the zoom by the defined zoom-factor.
    zoomFactor = qMax(0.1, ZoomConfig::zoomFactor());
    // Visibility of the mouse-pointer.
    mousePointer = MousePointerType(ZoomConfig::mousePointer());
    // Track moving of the mouse.
    mouseTracking = MouseTrackingType(ZoomConfig::mouseTracking());
#if HAVE_ACCESSIBILITY
    // Enable tracking of the focused location.
    m_accessibilityIntegration->setFocusTrackingEnabled(ZoomConfig::enableFocusTracking());
    // Enable tracking of the text caret.
    m_accessibilityIntegration->setTextCaretTrackingEnabled(ZoomConfig::enableTextCaretTracking());
#endif
    // The time in milliseconds to wait before a focus-event takes away a mouse-move.
    focusDelay = qMax(uint(0), ZoomConfig::focusDelay());
    // The factor the zoom-area will be moved on touching an edge on push-mode or using the navigation KAction's.
    moveFactor = qMax(0.1, ZoomConfig::moveFactor());
    if (source_zoom < 0) {
        // Load the saved zoom value.
        source_zoom = 1.0;
        target_zoom = ZoomConfig::initialZoom();
        if (target_zoom > 1.0)
            zoomIn(target_zoom);
    } else {
        source_zoom = 1.0;
    }
}

void ZoomEffect::prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
    if (zoom != target_zoom) {
        int time = 0;
        if (lastPresentTime.count())
            time = (presentTime - lastPresentTime).count();
        lastPresentTime = presentTime;

        const float zoomDist = qAbs(target_zoom - source_zoom);
        if (target_zoom > zoom)
            zoom = qMin(zoom + ((zoomDist * time) / animationTime(150*zoomFactor)), target_zoom);
        else
            zoom = qMax(zoom - ((zoomDist * time) / animationTime(150*zoomFactor)), target_zoom);
    }

    if (zoom == 1.0) {
        showCursor();
    } else {
        hideCursor();
        data.mask |= PAINT_SCREEN_TRANSFORMED;
    }

    effects->prePaintScreen(data, presentTime);
}

void ZoomEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    if (zoom != 1.0) {
        data *= QVector2D(zoom, zoom);
        const QSize screenSize = effects->virtualScreenSize();

        // mouse-tracking allows navigation of the zoom-area using the mouse.
        switch(mouseTracking) {
        case MouseTrackingProportional:
            data.setXTranslation(- int(cursorPoint.x() * (zoom - 1.0)));
            data.setYTranslation(- int(cursorPoint.y() * (zoom - 1.0)));
            prevPoint = cursorPoint;
            break;
        case MouseTrackingCentred:
            prevPoint = cursorPoint;
            // fall through
        case MouseTrackingDisabled:
            data.setXTranslation(qMin(0, qMax(int(screenSize.width() - screenSize.width() * zoom), int(screenSize.width() / 2 - prevPoint.x() * zoom))));
            data.setYTranslation(qMin(0, qMax(int(screenSize.height() - screenSize.height() * zoom), int(screenSize.height() / 2 - prevPoint.y() * zoom))));
            break;
        case MouseTrackingPush: {
                // touching an edge of the screen moves the zoom-area in that direction.
                int x = cursorPoint.x() * zoom - prevPoint.x() * (zoom - 1.0);
                int y = cursorPoint.y() * zoom - prevPoint.y() * (zoom - 1.0);
                int threshold = 4;
                xMove = yMove = 0;
                if (x < threshold)
                    xMove = (x - threshold) / zoom;
                else if (x + threshold > screenSize.width())
                    xMove = (x + threshold - screenSize.width()) / zoom;
                if (y < threshold)
                    yMove = (y - threshold) / zoom;
                else if (y + threshold > screenSize.height())
                    yMove = (y + threshold - screenSize.height()) / zoom;
                if (xMove)
                    prevPoint.setX(qMax(0, qMin(screenSize.width(), prevPoint.x() + xMove)));
                if (yMove)
                    prevPoint.setY(qMax(0, qMin(screenSize.height(), prevPoint.y() + yMove)));
                data.setXTranslation(- int(prevPoint.x() * (zoom - 1.0)));
                data.setYTranslation(- int(prevPoint.y() * (zoom - 1.0)));
                break;
            }
        }

        // use the focusPoint if focus tracking is enabled
        if (isFocusTrackingEnabled() || isTextCaretTrackingEnabled()) {
            bool acceptFocus = true;
            if (mouseTracking != MouseTrackingDisabled && focusDelay > 0) {
                // Wait some time for the mouse before doing the switch. This serves as threshold
                // to prevent the focus from jumping around to much while working with the mouse.
                const int msecs = lastMouseEvent.msecsTo(lastFocusEvent);
                acceptFocus = msecs > focusDelay;
            }
            if (acceptFocus) {
                data.setXTranslation(- int(focusPoint.x() * (zoom - 1.0)));
                data.setYTranslation(- int(focusPoint.y() * (zoom - 1.0)));
                prevPoint = focusPoint;
            }
        }
    }

    effects->paintScreen(mask, region, data);

    if (zoom != 1.0 && mousePointer != MousePointerHide) {
        GLTexture *cursorTexture = ensureCursorTexture();
        if (cursorTexture) {
            const auto cursor = effects->cursorImage();

            // Draw the mouse-texture at the position matching to zoomed-in image of the desktop. Hiding the
            // previous mouse-cursor and drawing our own fake mouse-cursor is needed to be able to scale the
            // mouse-cursor up and to re-position those mouse-cursor to match to the chosen zoom-level.
            QSize cursorSize = cursor.image().size() / cursor.image().devicePixelRatio();
            if (mousePointer == MousePointerScale) {
                cursorSize *= zoom;
            }

            const QPoint p = effects->cursorPos() - cursor.hotSpot();
            QRect rect(p * zoom + QPoint(data.xTranslation(), data.yTranslation()), cursorSize);

            cursorTexture->bind();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
            QMatrix4x4 mvp = data.projectionMatrix();
            mvp.translate(rect.x(), rect.y());
            s->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
            cursorTexture->render(region, rect);
            ShaderManager::instance()->popShader();
            cursorTexture->unbind();
            glDisable(GL_BLEND);
        }
    }
}

void ZoomEffect::postPaintScreen()
{
    if (zoom != target_zoom)
        effects->addRepaintFull();
    else
        lastPresentTime = std::chrono::milliseconds::zero();
    effects->postPaintScreen();
}

void ZoomEffect::zoomIn(double to)
{
    source_zoom = zoom;
    if (to < 0.0)
        target_zoom *= zoomFactor;
    else
        target_zoom = to;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    cursorPoint = effects->cursorPos();
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = cursorPoint;
    effects->addRepaintFull();
}

void ZoomEffect::zoomOut()
{
    source_zoom = zoom;
    target_zoom /= zoomFactor;
    if ((zoomFactor > 1 && target_zoom < 1.01) || (zoomFactor < 1 && target_zoom > 0.99)) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = effects->cursorPos();
    effects->addRepaintFull();
}

void ZoomEffect::actualSize()
{
    source_zoom = zoom;
    target_zoom = 1;
    if (polling) {
        polling = false;
        effects->stopMousePolling();
    }
    effects->addRepaintFull();
}

void ZoomEffect::timelineFrameChanged(int /* frame */)
{
    const QSize screenSize = effects->virtualScreenSize();
    prevPoint.setX(qMax(0, qMin(screenSize.width(), prevPoint.x() + xMove)));
    prevPoint.setY(qMax(0, qMin(screenSize.height(), prevPoint.y() + yMove)));
    cursorPoint = prevPoint;
    effects->addRepaintFull();
}

void ZoomEffect::moveZoom(int x, int y)
{
    if (timeline.state() == QTimeLine::Running)
        timeline.stop();

    const QSize screenSize = effects->virtualScreenSize();
    if (x < 0)
        xMove = - qMax(1.0, screenSize.width() / zoom / moveFactor);
    else if (x > 0)
        xMove = qMax(1.0, screenSize.width() / zoom  / moveFactor);
    else
        xMove = 0;

    if (y < 0)
        yMove = - qMax(1.0, screenSize.height() / zoom / moveFactor);
    else if (y > 0)
        yMove = qMax(1.0, screenSize.height() / zoom / moveFactor);
    else
        yMove = 0;

    timeline.start();
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
    QCursor::setPos(focusPoint.x(), focusPoint.y());
}

void ZoomEffect::moveMouseToCenter()
{
    const QRect r = effects->virtualScreenGeometry();
    QCursor::setPos(r.x() + r.width() / 2, r.y() + r.height() / 2);
}

void ZoomEffect::slotMouseChanged(const QPoint& pos, const QPoint& old, Qt::MouseButtons,
                              Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (zoom == 1.0)
        return;
    cursorPoint = pos;
    if (pos != old) {
        lastMouseEvent = QTime::currentTime();
        effects->addRepaintFull();
    }
}

void ZoomEffect::slotWindowDamaged()
{
    if (zoom != 1.0) {
        effects->addRepaintFull();
    }
}

void ZoomEffect::moveFocus(const QPoint &point)
{
    if (zoom == 1.0)
        return;
    focusPoint = point;
    lastFocusEvent = QTime::currentTime();
    effects->addRepaintFull();
}

bool ZoomEffect::isActive() const
{
    return zoom != 1.0 || zoom != target_zoom;
}

} // namespace

