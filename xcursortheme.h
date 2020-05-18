/*
 * Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <kwin_export.h>

#include <QImage>
#include <QMap>
#include <QVector>

#include <chrono>

namespace KWin
{

/**
 * The KXcursorSprite class represents a single sprite in the Xcursor theme.
 */
class KWIN_EXPORT KXcursorSprite
{
public:
    /**
     * Constructs an empty XcursorSprite.
     */
    KXcursorSprite();

    /**
     * Constructs an XcursorSprite with the specified @a data, @a hotspot, and @a delay.
     */
    KXcursorSprite(const QImage &data, const QPoint &hotspot,
                   const std::chrono::milliseconds &delay);

    /**
     * Returns the image for this sprite.
     */
    QImage data() const;

    /**
     * Returns the hotspot for this sprite. (0, 0) corresponds to the upper left corner.
     *
     * The coordinates of the hotspot are in device pixels.
     */
    QPoint hotspot() const;

    /**
     * Returns the time interval between this sprite and the next one, in milliseconds.
     */
    std::chrono::milliseconds delay() const;

private:
    QImage m_data;
    QPoint m_hotspot;
    std::chrono::milliseconds m_delay;
};

/**
 * The KXcursorTheme class represents an Xcursor theme.
 */
class KWIN_EXPORT KXcursorTheme
{
public:
    /**
     * Returns the ratio between device pixels and logical pixels for the Xcursor theme.
     */
    qreal devicePixelRatio() const;

    /**
     * Returns @c true if the Xcursor theme is empty; otherwise returns @c false.
     */
    bool isEmpty() const;

    /**
     * Returns the list of cursor sprites for the cursor with the given @a name.
     */
    QVector<KXcursorSprite> shape(const QByteArray &name) const;

    /**
     * Attempts to load the Xcursor theme with the given @a themeName and @a size.
     */
    static KXcursorTheme fromTheme(const QString &themeName, int size, qreal dpr);

private:
    QMap<QByteArray, QVector<KXcursorSprite>> m_cursorRegistry;
    qreal m_devicePixelRatio = 1;
};

} // namespace KWin
