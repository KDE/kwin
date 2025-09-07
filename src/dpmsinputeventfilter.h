/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input.h"

#include <QElapsedTimer>
#include <QObject>

#include <kwin_export.h>

class QProximitySensor;

namespace KWin
{

class DrmBackend;

class KWIN_EXPORT DpmsInputEventFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    DpmsInputEventFilter();
    ~DpmsInputEventFilter() override;

    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerAxis(PointerAxisEvent *event) override;
    bool keyboardKey(KeyboardKeyEvent *event) override;
    bool touchDown(TouchDownEvent *event) override;
    bool touchMotion(TouchMotionEvent *event) override;
    bool touchUp(TouchUpEvent *event) override;
    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override;
    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override;
    bool tabletToolTipEvent(TabletToolTipEvent *event) override;
    bool tabletToolButtonEvent(TabletToolButtonEvent *event) override;
    bool tabletPadButtonEvent(TabletPadButtonEvent *event) override;
    bool tabletPadStripEvent(TabletPadStripEvent *event) override;
    bool tabletPadRingEvent(TabletPadRingEvent *event) override;
    bool tabletPadDialEvent(TabletPadDialEvent *event) override;

private Q_SLOTS:
    void updateProximitySensor();

private:
    void notify();
    QElapsedTimer m_doubleTapTimer;
    QList<qint32> m_touchPoints;
    std::unique_ptr<QProximitySensor> m_sensor;

    bool m_secondTap = false;
    bool m_enableDoubleTap;
    bool m_proximityClose = false;
};

}
