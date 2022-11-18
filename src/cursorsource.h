/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/xcursortheme.h"

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QTimer>

namespace KWin
{

/**
 * The CursorSource class represents the contents of the Cursor.
 */
class KWIN_EXPORT CursorSource : public QObject
{
    Q_OBJECT

public:
    explicit CursorSource(QObject *parent = nullptr);

    QImage image() const;
    QPoint hotspot() const;

Q_SIGNALS:
    void changed();

protected:
    QImage m_image;
    QPoint m_hotspot;
};

/**
 * The ImageCursorSource class represents a static raster cursor pixmap.
 */
class KWIN_EXPORT ImageCursorSource : public CursorSource
{
    Q_OBJECT

public:
    explicit ImageCursorSource(QObject *parent = nullptr);

public Q_SLOTS:
    void update(const QImage &image, const QPoint &hotspot);
};

/**
 * The ShapeCursorSource class represents the contents of a shape in the cursor theme.
 */
class KWIN_EXPORT ShapeCursorSource : public CursorSource
{
    Q_OBJECT

public:
    explicit ShapeCursorSource(QObject *parent = nullptr);

    QByteArray shape() const;
    void setShape(const QByteArray &shape);
    void setShape(Qt::CursorShape shape);

    KXcursorTheme theme() const;
    void setTheme(const KXcursorTheme &theme);

private:
    void refresh();
    void selectNextSprite();
    void selectSprite(int index);

    KXcursorTheme m_theme;
    QByteArray m_shape;
    QVector<KXcursorSprite> m_sprites;
    QTimer m_delayTimer;
    int m_currentSprite = -1;
};

} // namespace KWin
