/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input.h"

#include <QElapsedTimer>

#include <kwin_export.h>

namespace KWin
{

class DrmBackend;

class KWIN_EXPORT DpmsInputEventFilter : public InputEventFilter
{
public:
    DpmsInputEventFilter();
    ~DpmsInputEventFilter() override;

    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(WheelEvent *event) override;
    bool keyEvent(KeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;

private:
    void notify();
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    bool m_secondTap = false;
    bool m_enableDoubleTap;
};

}
