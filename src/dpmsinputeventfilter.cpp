/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dpmsinputeventfilter.h"
#include "core/output.h"
#include "input_event.h"
#include "main.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QGuiApplication>
#include <QKeyEvent>

namespace KWin
{

DpmsInputEventFilter::DpmsInputEventFilter()
    : InputEventFilter()
{
    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    m_enableDoubleTap = kwinSettings->group("Wayland").readEntry<bool>("DoubleTapWakeup", true);
}

DpmsInputEventFilter::~DpmsInputEventFilter() = default;

bool DpmsInputEventFilter::pointerEvent(MouseEvent *event, quint32 nativeButton)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::wheelEvent(WheelEvent *event)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::keyEvent(KeyEvent *event)
{
    if (event->type() == QKeyEvent::KeyPress) {
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
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

bool DpmsInputEventFilter::touchUp(qint32 id, std::chrono::microseconds time)
{
    if (m_enableDoubleTap) {
        m_touchPoints.removeAll(id);
        if (m_touchPoints.isEmpty() && m_doubleTapTimer.isValid() && m_secondTap) {
            if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                waylandServer()->seat()->setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time));
                notify();
            }
            m_doubleTapTimer.invalidate();
            m_secondTap = false;
        }
    }
    return true;
}

bool DpmsInputEventFilter::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    // ignore the event
    return true;
}

void DpmsInputEventFilter::notify()
{
    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        output->setDpmsMode(Output::DpmsMode::On);
    }
}

}
