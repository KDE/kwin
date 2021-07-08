/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dpmsinputeventfilter.h"
#include "platform.h"
#include "abstract_wayland_output.h"
#include "wayland_server.h"
#include "main.h"

#include <QGuiApplication>
#include <QKeyEvent>

#include <KWaylandServer/seat_interface.h>

namespace KWin
{

DpmsInputEventFilter::DpmsInputEventFilter()
    : InputEventFilter()
{
    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    m_enableDoubleTap = kwinSettings->group("Wayland").readEntry<bool>("DoubleTapWakeup", true);
}

DpmsInputEventFilter::~DpmsInputEventFilter() = default;

bool DpmsInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    notify();
    return true;
}

bool DpmsInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    notify();
    return true;
}

bool DpmsInputEventFilter::keyEvent(QKeyEvent *event)
{
    if (event->type() == QKeyEvent::KeyPress) {
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(pos)
    Q_UNUSED(time)
    if (m_enableDoubleTap) {
        if (m_touchPoints.isEmpty()) {
            if (!m_doubleTapTimer.isValid()) {
                // this is the first tap
                m_doubleTapTimer.start();
            } else {
                if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                    m_secondTap = true;
                } else {
                    // took too long. Let's consider it a new click
                    m_doubleTapTimer.restart();
                }
            }
        } else {
            // not a double tap
            m_doubleTapTimer.invalidate();
            m_secondTap = false;
        }
        m_touchPoints << id;
    }
    return true;
}

bool DpmsInputEventFilter::touchUp(qint32 id, quint32 time)
{
    if (m_enableDoubleTap) {
        m_touchPoints.removeAll(id);
        if (m_touchPoints.isEmpty() && m_doubleTapTimer.isValid() && m_secondTap) {
            if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                waylandServer()->seat()->setTimestamp(time);
                notify();
            }
            m_doubleTapTimer.invalidate();
            m_secondTap = false;
        }
    }
    return true;
}

bool DpmsInputEventFilter::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    // ignore the event
    return true;
}

void DpmsInputEventFilter::notify()
{
    const QVector<AbstractOutput *> enabledOutputs = kwinApp()->platform()->enabledOutputs();
    for (auto it = enabledOutputs.constBegin(), end = enabledOutputs.constEnd(); it != end; it++) {
        auto waylandOutput = static_cast<AbstractWaylandOutput *>(*it);
        waylandOutput->setDpmsMode(AbstractWaylandOutput::DpmsMode::On);
    }
}

}
