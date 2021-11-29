/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_TOUCH_INPUT_H
#define KWIN_TOUCH_INPUT_H
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPointF>

namespace KWin
{

class InputDevice;
class InputRedirection;
class Toplevel;

namespace Decoration
{
class DecoratedClientImpl;
}

class TouchInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TouchInputRedirection(InputRedirection *parent);
    ~TouchInputRedirection() override;

    bool positionValid() const override;
    bool focusUpdatesBlocked() override;
    void init() override;

    void processDown(qint32 id, const QPointF &pos, quint32 time, InputDevice *device = nullptr);
    void processUp(qint32 id, quint32 time, InputDevice *device = nullptr);
    void processMotion(qint32 id, const QPointF &pos, quint32 time, InputDevice *device = nullptr);
    void cancel();
    void frame();

    void setDecorationPressId(qint32 id) {
        m_decorationId = id;
    }
    qint32 decorationPressId() const {
        return m_decorationId;
    }
    void setInternalPressId(qint32 id) {
        m_internalId = id;
    }
    qint32 internalPressId() const {
        return m_internalId;
    }

    QPointF position() const override {
        return m_lastPosition;
    }

    int touchPointCount() const {
        return m_activeTouchPoints.count();
    }

private:
    void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) override;

    void focusUpdate(Toplevel *focusOld, Toplevel *focusNow) override;

    QSet<qint32> m_activeTouchPoints;
    bool m_inited = false;
    qint32 m_decorationId = -1;
    qint32 m_internalId = -1;
    QMetaObject::Connection m_focusGeometryConnection;
    bool m_windowUpdatedInCycle = false;
    QPointF m_lastPosition;
};

}

#endif
