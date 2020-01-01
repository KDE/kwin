/*
 * Copyright 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "windowshadow.h"

#include <QVariant>

Q_DECLARE_METATYPE(QMargins)

namespace KWin
{

bool WindowShadowTile::create()
{
    return true;
}

void WindowShadowTile::destroy()
{
}

bool WindowShadow::create()
{
    // TODO: Perhaps we set way too many properties here. Alternatively we could put all shadow tiles
    // in one big image and attach it rather than 8 separate images.
    if (leftTile) {
        window->setProperty("kwin_shadow_left_tile", QVariant::fromValue(leftTile->image()));
    }
    if (topLeftTile) {
        window->setProperty("kwin_shadow_top_left_tile", QVariant::fromValue(topLeftTile->image()));
    }
    if (topTile) {
        window->setProperty("kwin_shadow_top_tile", QVariant::fromValue(topTile->image()));
    }
    if (topRightTile) {
        window->setProperty("kwin_shadow_top_right_tile", QVariant::fromValue(topRightTile->image()));
    }
    if (rightTile) {
        window->setProperty("kwin_shadow_right_tile", QVariant::fromValue(rightTile->image()));
    }
    if (bottomRightTile) {
        window->setProperty("kwin_shadow_bottom_right_tile", QVariant::fromValue(bottomRightTile->image()));
    }
    if (bottomTile) {
        window->setProperty("kwin_shadow_bottom_tile", QVariant::fromValue(bottomTile->image()));
    }
    if (bottomLeftTile) {
        window->setProperty("kwin_shadow_bottom_left_tile", QVariant::fromValue(bottomLeftTile->image()));
    }
    window->setProperty("kwin_shadow_padding", QVariant::fromValue(padding));

    // Notice that the enabled property must be set last.
    window->setProperty("kwin_shadow_enabled", QVariant::fromValue(true));

    return true;
}

void WindowShadow::destroy()
{
    // Attempting to uninstall the shadow after the decorated window has been destroyed. It's doomed.
    if (!window) {
        return;
    }

    // Remove relevant shadow properties.
    window->setProperty("kwin_shadow_left_tile", {});
    window->setProperty("kwin_shadow_top_left_tile", {});
    window->setProperty("kwin_shadow_top_tile", {});
    window->setProperty("kwin_shadow_top_right_tile", {});
    window->setProperty("kwin_shadow_right_tile", {});
    window->setProperty("kwin_shadow_bottom_right_tile", {});
    window->setProperty("kwin_shadow_bottom_tile", {});
    window->setProperty("kwin_shadow_bottom_left_tile", {});
    window->setProperty("kwin_shadow_padding", {});
    window->setProperty("kwin_shadow_enabled", {});
}

} // namespace KWin
