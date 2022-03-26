/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_TABLET_INPUT_H
#define KWIN_TABLET_INPUT_H
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>

namespace KWin
{
class Toplevel;
class TabletToolId;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
class Device;
}

class TabletInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TabletInputRedirection(InputRedirection *parent);
    ~TabletInputRedirection() override;

    void tabletPad();
    bool focusUpdatesBlocked() override;

    void tabletToolEvent(KWin::InputRedirection::TabletEventType type, const QPointF &pos,
                         qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                         bool tipNear, const TabletToolId &tabletToolId,
                         quint32 time);
    void tabletToolButtonEvent(uint button, bool isPressed, const TabletToolId &tabletToolId);

    void tabletPadButtonEvent(uint button, bool isPressed, const TabletPadId &tabletPadId);
    void tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId);
    void tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId);

    bool positionValid() const override
    {
        return !m_lastPosition.isNull();
    }
    void init() override;

    QPointF position() const override
    {
        return m_lastPosition;
    }

private:
    void cleanupDecoration(Decoration::DecoratedClientImpl *old,
                           Decoration::DecoratedClientImpl *now) override;
    void focusUpdate(Toplevel *focusOld, Toplevel *focusNow) override;

    bool m_tipDown = false;
    bool m_tipNear = false;

    QPointF m_lastPosition;
    QMetaObject::Connection m_decorationGeometryConnection;
    QMetaObject::Connection m_decorationDestroyedConnection;
};

}

#endif
