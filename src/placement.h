/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 1997-2002 Cristian Tibirna <tibirna@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
// KWin
#include "options.h"
#include "window.h"
// Qt
#include <QList>
#include <QPoint>
#include <QRect>

namespace KWin
{

class VirtualDesktop;

class KWIN_EXPORT Placement
{
public:
    explicit Placement();

    std::optional<PlacementCommand> place(Window *c, const QRectF &area);
    std::optional<PlacementCommand> placeSmart(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeCentered(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

    void reinitCascading();
    void reinitCascading(VirtualDesktop *desktop);

    QRectF cascadeIfCovering(Window *c, const QRectF &geometry, const QRectF &area) const;

    /**
     * Cascades all clients on the current desktop
     */
    void cascadeDesktop();
    /**
     *   Unclutters the current desktop by smart-placing all clients again.
     */
    void unclutterDesktop();

    static const char *policyToString(PlacementPolicy policy);

private:
    std::optional<PlacementCommand> place(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement = PlacementUnknown);
    std::optional<PlacementCommand> placeUnderMouse(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeOnMainWindow(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeTransient(Window *c);
    std::optional<PlacementCommand> placeAtRandom(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeCascaded(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeMaximizing(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeZeroCornered(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeDialog(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeUtility(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeOnScreenDisplay(Window *c, const QRect &area);

    // CT needed for cascading+
    struct DesktopCascadingInfo
    {
        QPoint pos;
        int col;
        int row;
    };

    QHash<VirtualDesktop *, DesktopCascadingInfo> cci;
};

} // namespace
