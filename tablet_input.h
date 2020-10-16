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

    void tabletToolEvent(KWin::InputRedirection::TabletEventType type, const QPointF &pos,
                         qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                         bool tipNear, quint64 serialId, quint64 toolId,
                         InputRedirection::TabletToolType toolType, const QVector<InputRedirection::Capability> &capabilities,
                         quint32 time, LibInput::Device *device);
    void tabletToolButtonEvent(uint button, bool isPressed);

    void tabletPadButtonEvent(uint button, bool isPressed);
    void tabletPadStripEvent(int number, int position, bool isFinger);
    void tabletPadRingEvent(int number, int position, bool isFinger);

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
    void cleanupInternalWindow(QWindow *old, QWindow *now) override;
    void focusUpdate(KWin::Toplevel *old, KWin::Toplevel *now) override;

    bool m_tipDown = false;
    bool m_tipNear = false;

    QPointF m_lastPosition;
    QSet<uint> m_toolPressedButtons;
    QSet<uint> m_padPressedButtons;
};

}

#endif
