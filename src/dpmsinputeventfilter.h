/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DPMSINPUTEVENTFILTER_H
#define KWIN_DPMSINPUTEVENTFILTER_H
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

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

private:
    void notify();
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    bool m_secondTap = false;
    bool m_enableDoubleTap;
};

}


#endif

