/*
    SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/reflectcursor/reflectcursor.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "plugins/reflectcursor/reflectcursorconfig.h"
#include "pointer_input.h"
#include "scene/cursoritem.h"
#include "scene/workspacescene.h"
#include "workspace.h"

namespace KWin
{

ReflectCursorEffect::ReflectCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    ReflectCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

ReflectCursorEffect::~ReflectCursorEffect()
{
    hideEffect();
}

bool ReflectCursorEffect::supported()
{
    return effects->waylandDisplay() && effects->isOpenGLCompositing();
}

void ReflectCursorEffect::reconfigure(ReconfigureFlags flags)
{
    ReflectCursorConfig::self()->read();

    m_threshold = ReflectCursorConfig::threshold();
}

bool ReflectCursorEffect::isActive() const
{
    return m_reflectX || m_reflectY;
}

void ReflectCursorEffect::pointerEvent(MouseEvent *event)
{
    if (event->type() != QEvent::MouseMove) {
        return;
    }

    if (input()->pointer()->isConstrained()) {
        return;
    }

    if (effects->isCursorHidden() && !m_cursorItem) {
        // something else is hiding the cursor, don't interfere
        return;
    }

    update(m_cursor->geometry(), m_cursor->hotspot(), workspace()->outputAt(m_cursor->pos())->geometryF());
}

void ReflectCursorEffect::hideEffect()
{
    if (m_cursorItem) {
        m_cursorItem.reset();
        effects->showCursor();
    }
}

void ReflectCursorEffect::showEffect()
{
    if (!m_cursorItem) {
        effects->hideCursor();
        m_cursorItem = std::make_unique<CursorItem>(effects->scene()->overlayItem());
        m_cursorItem->setParentItem(effects->scene()->overlayItem());
        m_cursorItem->setPosition(m_cursor->pos());
        connect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), [this]() {
            m_cursorItem->setPosition(m_cursor->pos());
        });
    }
    m_cursorItem->setTransform(QTransform::fromScale(m_reflectX ? -1 : 1, m_reflectY ? -1 : 1));
}

static qreal area(const QRectF &rect)
{
    return rect.width() * rect.height();
}

static QRectF reflectX(const QRectF &rect, qreal x)
{
    return QRectF(2 * x - rect.right(), rect.y(), rect.width(), rect.height());
}

static QRectF reflectY(const QRectF &rect, qreal y)
{
    return QRectF(rect.x(), 2 * y - rect.bottom(), rect.width(), rect.height());
}

void ReflectCursorEffect::update(const QRectF &cursorGeometry, const QPointF &hotspot, const QRectF &screenGeometry)
{
    const bool wasReflectX = m_reflectX;
    const bool wasReflectY = m_reflectY;
    m_reflectX = false;
    m_reflectY = false;
    qreal cursorSize = area(cursorGeometry);
    QRectF cursorOnScreen = screenGeometry & cursorGeometry;
    qreal cursorOnScreenSize = area(cursorOnScreen);
    if (cursorOnScreenSize < cursorSize * (1 - m_threshold)) {
        const QRectF cursorReflectX = reflectX(cursorGeometry, cursorGeometry.x() + hotspot.x());
        const qreal cursorReflectXArea = area(cursorReflectX & screenGeometry);
        // extra 10% to prefer original orientation in case of near equal areas
        constexpr qreal ratio = 1.1;
        QRectF newGeometry = cursorGeometry;
        if (cursorReflectXArea > cursorOnScreenSize * ratio) {
            newGeometry = cursorReflectX;
            m_reflectX = true;
            cursorOnScreenSize = cursorReflectXArea;
        }
        const QRectF cursorReflectY = reflectY(newGeometry, newGeometry.y() + hotspot.y());
        if (area(cursorReflectY & screenGeometry) > cursorOnScreenSize * ratio) {
            m_reflectY = true;
        }
    }

    if (wasReflectX != m_reflectX || wasReflectY != m_reflectY) {
        if (isActive()) {
            showEffect();
        } else {
            hideEffect();
        }
    }
}

} // namespace KWin

#include "moc_reflectcursor.cpp"
