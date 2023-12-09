/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "plugins/shakecursor/shakecursorconfig.h"
#include "pointer_input.h"
#include "scene/cursoritem.h"
#include "scene/workspacescene.h"

namespace KWin
{

ShakeCursorEffect::ShakeCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    m_resetCursorScaleTimer.setSingleShot(true);
    connect(&m_resetCursorScaleTimer, &QTimer::timeout, this, [this]() {
        m_resetCursorScaleAnimation.setStartValue(m_cursorMagnification);
        m_resetCursorScaleAnimation.setEndValue(1.0);
        m_resetCursorScaleAnimation.setDuration(animationTime(150));
        m_resetCursorScaleAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        m_resetCursorScaleAnimation.start();
    });

    connect(&m_resetCursorScaleAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        magnify(m_resetCursorScaleAnimation.currentValue().toReal());
    });

    ShakeCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

ShakeCursorEffect::~ShakeCursorEffect()
{
    magnify(1.0);
}

bool ShakeCursorEffect::supported()
{
    if (!effects->waylandDisplay()) {
        return false;
    }
    return effects->isOpenGLCompositing();
}

void ShakeCursorEffect::reconfigure(ReconfigureFlags flags)
{
    ShakeCursorConfig::self()->read();

    m_shakeDetector.setInterval(ShakeCursorConfig::timeInterval());
    m_shakeDetector.setSensitivity(ShakeCursorConfig::sensitivity());
}

bool ShakeCursorEffect::isActive() const
{
    return m_cursorMagnification != 1.0;
}

void ShakeCursorEffect::pointerEvent(MouseEvent *event)
{
    if (event->type() != QEvent::MouseMove || event->buttons() != Qt::NoButton) {
        return;
    }

    if (input()->pointer()->isConstrained()) {
        return;
    }

    if (const auto shakeFactor = m_shakeDetector.update(event)) {
        m_resetCursorScaleTimer.start(animationTime(2000));
        m_resetCursorScaleAnimation.stop();

        magnify(std::max(m_cursorMagnification, 1.0 + ShakeCursorConfig::magnification() * shakeFactor.value()));
    }
}

void ShakeCursorEffect::magnify(qreal magnification)
{
    if (magnification == 1.0) {
        m_cursorMagnification = 1.0;
        if (m_cursorItem) {
            m_cursorItem.reset();
            effects->showCursor();
        }
    } else {
        m_cursorMagnification = magnification;

        if (!m_cursorItem) {
            effects->hideCursor();

            m_cursorItem = std::make_unique<CursorItem>(effects->scene());
            m_cursorItem->setParentItem(effects->scene()->overlayItem());
            m_cursorItem->setPosition(m_cursor->pos());
            connect(m_cursor, &Cursor::posChanged, m_cursorItem.get(), [this]() {
                m_cursorItem->setPosition(m_cursor->pos());
            });
        }
        m_cursorItem->setTransform(QTransform::fromScale(magnification, magnification));
    }
}

} // namespace KWin

#include "moc_shakecursor.cpp"
