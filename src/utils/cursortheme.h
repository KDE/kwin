/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QImage>
#include <QList>
#include <QSharedDataPointer>

#include <chrono>

namespace KWin
{

class CursorSpritePrivate;
class CursorThemePrivate;

/**
 * The CursorSprite class represents a single sprite in the cursor theme.
 */
class KWIN_EXPORT CursorSprite
{
public:
    /**
     * Constructs an empty CursorSprite.
     */
    CursorSprite();

    /**
     * Constructs a copy of the CursorSprite object @a other.
     */
    CursorSprite(const CursorSprite &other);

    /**
     * Constructs an CursorSprite with the specified @a data, @a hotspot, and @a delay.
     */
    CursorSprite(const QImage &data, const QPoint &hotspot, const std::chrono::milliseconds &delay);

    /**
     * Destructs the CursorSprite object.
     */
    ~CursorSprite();

    /**
     * Assigns the value of @a other to the cursor sprite object.
     */
    CursorSprite &operator=(const CursorSprite &other);

    /**
     * Returns the image for this sprite.
     */
    QImage data() const;

    /**
     * Returns the hotspot for this sprite. (0, 0) corresponds to the upper left corner.
     *
     * The coordinates of the hotspot are in device independent pixels.
     */
    QPoint hotspot() const;

    /**
     * Returns the time interval between this sprite and the next one, in milliseconds.
     */
    std::chrono::milliseconds delay() const;

private:
    QSharedDataPointer<CursorSpritePrivate> d;
};

/**
 * The CursorTheme class represents a cursor theme.
 */
class KWIN_EXPORT CursorTheme
{
public:
    /**
     * Constructs an empty cursor theme.
     */
    CursorTheme();

    /**
     * Loads the cursor theme with the given @ themeName and the desired @a size.
     * The @a dpr specifies the desired scale factor. If no theme with the provided
     * name exists, the cursor theme will be empty.
     *
     * @a searchPaths specifies where the cursor theme should be looked for.
     */
    CursorTheme(const QString &theme, int size, qreal devicePixelRatio, const QStringList &searchPaths = QStringList());

    /**
     * Constructs a copy of the CursorTheme object @a other.
     */
    CursorTheme(const CursorTheme &other);

    /**
     * Destructs the CursorTheme object.
     */
    ~CursorTheme();

    /**
     * Assigns the value of @a other to the cursor theme object.
     */
    CursorTheme &operator=(const CursorTheme &other);

    bool operator==(const CursorTheme &other);
    bool operator!=(const CursorTheme &other);

    /**
     * The name of the requested cursor theme.
     */
    QString name() const;

    /**
     * The size of the requested cursor theme.
     */
    int size() const;

    /**
     * The scale factor of the requested cursor theme.
     */
    qreal devicePixelRatio() const;

    /**
     * Returns @c true if the cursor theme is empty; otherwise returns @c false.
     */
    bool isEmpty() const;

    /**
     * Returns the list of cursor sprites for the cursor with the given @a name.
     */
    QList<CursorSprite> shape(const QByteArray &name) const;

private:
    QSharedDataPointer<CursorThemePrivate> d;
};

} // namespace KWin
