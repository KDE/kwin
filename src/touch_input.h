/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>

namespace KWin
{

class InputDevice;
class InputRedirection;
class Window;

namespace Decoration
{
class DecoratedClientImpl;
}

class KWIN_EXPORT TouchInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TouchInputRedirection(InputRedirection *parent);
    ~TouchInputRedirection() override;

    bool positionValid() const override;
    bool focusUpdatesBlocked() override;
    void init() override;

    void processDown(qint32 id, const QPointF &pos, std::chrono::microseconds time, InputDevice *device = nullptr);
    void processUp(qint32 id, std::chrono::microseconds time, InputDevice *device = nullptr);
    void processMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time, InputDevice *device = nullptr);
    void cancel();
    void frame();

    void setDecorationPressId(qint32 id)
    {
        m_decorationId = id;
    }
    qint32 decorationPressId() const
    {
        return m_decorationId;
    }
    void setInternalPressId(qint32 id)
    {
        m_internalId = id;
    }
    qint32 internalPressId() const
    {
        return m_internalId;
    }

    QPointF position() const override
    {
        return m_lastPosition;
    }

    int touchPointCount() const
    {
        return m_activeTouchPoints.count();
    }

private:
    void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) override;

    void focusUpdate(Window *focusOld, Window *focusNow) override;

    QSet<qint32> m_activeTouchPoints;
    bool m_inited = false;
    qint32 m_decorationId = -1;
    qint32 m_internalId = -1;
    QMetaObject::Connection m_focusGeometryConnection;
    bool m_windowUpdatedInCycle = false;
    QPointF m_lastPosition;
};

}
