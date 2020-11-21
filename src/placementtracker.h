/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QRect>
#include <QString>

namespace KWin
{

class AbstractClient;

/**
 * The PlacementTracker class is a helper that tries to put windows back on their original
 * screens if the screen layout changes.
 *
 * The PlacementTracker monitors the placement of windows. Two states are associated with every
 * window - user and last.
 *
 * The last state indicates the position of the window where it has been seen last time.
 *
 * The user state tracks the placement of the window as expected by user. For example, if the
 * output where the window is on has been disconnected and later re-connected, this state will
 * contain information needed to place the window back on its original output.
 *
 * The user state is updated in response to user actions such as move/resize, maximize, etc.
 */
class PlacementTracker : public QObject
{
    Q_OBJECT

public:
    explicit PlacementTracker(QObject *parent = nullptr);

    /**
     * The State enum is used to describe a state tracked by the tracker.
     */
    enum class State : uint {
        User = 0x1, /// The placement of the window as expected by the user
        Last = 0x2, /// The last seen placement of the window
    };
    Q_DECLARE_FLAGS(States, State)

    /**
     * Asks the tracker to synchronize the specified @a states for the given @a client.
     */
    void update(AbstractClient *client, States states = States());

    /**
     * Asks the tracker to rearrange all windows in the workspace according to tracked state.
     */
    void rearrange();

private Q_SLOTS:
    void handleClientAdded(AbstractClient *client);
    void handleClientRemoved(AbstractClient *client);

private:
    struct PlacementState
    {
        QRect outputGeometry;
        QRect geometry;
        QString outputName;
    };

    struct PlacementData
    {
        PlacementState user;
        PlacementState last;
    };

    QHash<AbstractClient *, PlacementData> m_placementData;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::PlacementTracker::States)
