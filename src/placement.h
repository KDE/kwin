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

class KWIN_EXPORT Placement
{
public:
    explicit Placement();

    std::optional<PlacementCommand> place(const Window *c, const QRectF &area);
    std::optional<PlacementCommand> placeSmart(const Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeCentered(const Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

    QRectF cascadeIfCovering(const Window *c, const QRectF &geometry, const QRectF &area) const;

    static const char *policyToString(PlacementPolicy policy);

private:
    std::optional<PlacementCommand> place(const Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement = PlacementUnknown);
    std::optional<PlacementCommand> placeUnderMouse(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeOnMainWindow(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeAtRandom(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeMaximizing(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeZeroCornered(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeDialog(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeUtility(const Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    std::optional<PlacementCommand> placeOnScreenDisplay(const Window *c, const QRect &area);
    std::optional<PlacementCommand> placePictureInPicture(const Window *c, const QRect &area);
};

} // namespace
