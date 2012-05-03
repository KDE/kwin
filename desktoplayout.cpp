/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "workspace.h"
#include "options.h"

#include "assert.h"

namespace KWin
{

void Workspace::updateDesktopLayout()
{
#ifdef KWIN_BUILD_SCREENEDGES
    if (options->electricBorders() == Options::ElectricAlways) {
        m_screenEdge.reserveDesktopSwitching(false, m_screenEdgeOrientation);
    }
#endif
    // TODO: Is there a sane way to avoid overriding the existing grid?
    int width = rootInfo->desktopLayoutColumnsRows().width();
    int height = rootInfo->desktopLayoutColumnsRows().height();
    if (width == 0 && height == 0)   // Not given, set default layout
        height = 2;
    setNETDesktopLayout(
        rootInfo->desktopLayoutOrientation() == NET::OrientationHorizontal ? Qt::Horizontal : Qt::Vertical,
        width, height, 0 //rootInfo->desktopLayoutCorner() // Not really worth implementing right now.
    );

#ifdef KWIN_BUILD_SCREENEDGES
    m_screenEdgeOrientation = 0;
    if (width > 1)
        m_screenEdgeOrientation |= Qt::Horizontal;
    if (height > 1)
        m_screenEdgeOrientation |= Qt::Vertical;
    if (options->electricBorders() == Options::ElectricAlways) {
        m_screenEdge.reserveDesktopSwitching(true, m_screenEdgeOrientation);
    }
#endif
}

void Workspace::setNETDesktopLayout(Qt::Orientation orientation, int width, int height,
                                    int startingCorner)
{
    Q_UNUSED(startingCorner);   // Not really worth implementing right now.

    // Calculate valid grid size
    assert(width > 0 || height > 0);
    if ((width <= 0) && (height > 0))
        width = (desktopCount_ + height - 1) / height;
    else if ((height <= 0) && (width > 0))
        height = (desktopCount_ + width - 1) / width;
    while (width * height < desktopCount_) {
        if (orientation == Qt::Horizontal)
            ++width;
        else
            ++height;
    }

    // Set private variables
    delete[] desktopGrid_;
    desktopGridSize_ = QSize(width, height);
    int size = width * height;
    desktopGrid_ = new int[size];

    // Populate grid
    int desktop = 1;
    if (orientation == Qt::Horizontal)
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                desktopGrid_[y * width + x] = (desktop <= desktopCount_ ? desktop++ : 0);
    else
        for (int x = 0; x < width; x++)
            for (int y = 0; y < height; y++)
                desktopGrid_[y * width + x] = (desktop <= desktopCount_ ? desktop++ : 0);
}

QPoint Workspace::desktopGridCoords(int id) const
{
    for (int y = 0; y < desktopGridSize_.height(); y++)
        for (int x = 0; x < desktopGridSize_.width(); x++)
            if (desktopGrid_[y * desktopGridSize_.width() + x] == id)
                return QPoint(x, y);
    return QPoint(-1, -1);
}

QPoint Workspace::desktopCoords(int id) const
{
    QPoint coords = desktopGridCoords(id);
    if (coords.x() == -1)
        return QPoint(-1, -1);
    return QPoint(coords.x() * displayWidth(), coords.y() * displayHeight());
}

int Workspace::desktopAbove(int id, bool wrap) const
{
    if (id == 0)
        id = currentDesktop();
    QPoint coords = desktopGridCoords(id);
    assert(coords.x() >= 0);
    for (;;) {
        coords.ry()--;
        if (coords.y() < 0) {
            if (wrap)
                coords.setY(desktopGridSize_.height() - 1);
            else
                return id; // Already at the top-most desktop
        }
        int desktop = desktopAtCoords(coords);
        if (desktop > 0)
            return desktop;
    }
}

int Workspace::desktopToRight(int id, bool wrap) const
{
    if (id == 0)
        id = currentDesktop();
    QPoint coords = desktopGridCoords(id);
    assert(coords.x() >= 0);
    for (;;) {
        coords.rx()++;
        if (coords.x() >= desktopGridSize_.width()) {
            if (wrap)
                coords.setX(0);
            else
                return id; // Already at the right-most desktop
        }
        int desktop = desktopAtCoords(coords);
        if (desktop > 0)
            return desktop;
    }
}

int Workspace::desktopBelow(int id, bool wrap) const
{
    if (id == 0)
        id = currentDesktop();
    QPoint coords = desktopGridCoords(id);
    assert(coords.x() >= 0);
    for (;;) {
        coords.ry()++;
        if (coords.y() >= desktopGridSize_.height()) {
            if (wrap)
                coords.setY(0);
            else
                return id; // Already at the bottom-most desktop
        }
        int desktop = desktopAtCoords(coords);
        if (desktop > 0)
            return desktop;
    }
}

int Workspace::desktopToLeft(int id, bool wrap) const
{
    if (id == 0)
        id = currentDesktop();
    QPoint coords = desktopGridCoords(id);
    assert(coords.x() >= 0);
    for (;;) {
        coords.rx()--;
        if (coords.x() < 0) {
            if (wrap)
                coords.setX(desktopGridSize_.width() - 1);
            else
                return id; // Already at the left-most desktop
        }
        int desktop = desktopAtCoords(coords);
        if (desktop > 0)
            return desktop;
    }
}

} // namespace
