/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QImage>
#include <QObject>
#include <QPoint>

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

} // namespace KWin
