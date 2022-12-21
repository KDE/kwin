/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/xcursortheme.h"

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QTimer>

namespace KWaylandServer
{
class SurfaceInterface;
}

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
    QSize size() const;
    QPoint hotspot() const;

Q_SIGNALS:
    void changed();

protected:
    QImage m_image;
    QSize m_size = QSize(0, 0);
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

/**
 * The SurfaceCursorSource class repsents the contents of a cursor backed by a wl_surface.
 */
class KWIN_EXPORT SurfaceCursorSource : public CursorSource
{
    Q_OBJECT

public:
    explicit SurfaceCursorSource(QObject *parent = nullptr);

    KWaylandServer::SurfaceInterface *surface() const;

public Q_SLOTS:
    void update(KWaylandServer::SurfaceInterface *surface, const QPoint &hotspot);

private:
    QPointer<KWaylandServer::SurfaceInterface> m_surface;
};

} // namespace KWin
