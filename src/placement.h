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
// Qt
#include <QList>
#include <QPoint>
#include <QRect>

namespace KWin
{

class VirtualDesktop;
class Window;

class KWIN_EXPORT Placement
{
public:
    explicit Placement();

    bool place(Window *c, const QRectF &area);
    bool placeSmart(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

    bool placeCentered(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

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
    bool place(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement = PlacementUnknown);
    bool placeUnderMouse(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeOnMainWindow(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeTransient(Window *c);
    bool placeAtRandom(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeCascaded(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeMaximizing(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeZeroCornered(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeDialog(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeUtility(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    bool placeOnScreenDisplay(Window *c, const QRect &area);

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
