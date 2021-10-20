/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QMatrix4x4>
#include <QObject>
#include <QPointer>

namespace KWin
{

class Deleted;
class SurfacePixmap;
class SurfaceView;
class Toplevel;

/**
 * The Surface class represents the contents of a window.
 */
class KWIN_EXPORT Surface : public QObject
{
    Q_OBJECT

public:
    explicit Surface(Toplevel *window, QObject *parent = nullptr);

    virtual QRegion shape() const;
    virtual QRegion opaque() const;

    virtual SurfacePixmap *createPixmap() = 0;

    Toplevel *window() const;

    QMatrix4x4 surfaceToBufferMatrix() const;
    void setSurfaceToBufferMatrix(const QMatrix4x4 &matrix);

    QPoint position() const;
    void setPosition(const QPoint &pos);

    QSize size() const;
    void setSize(const QSize &size);

    bool isMapped() const;
    void setMapped(bool mapped);

    QList<Surface *> below() const;
    void setBelow(const QList<Surface *> &below);

    QList<Surface *> above() const;
    void setAbove(const QList<Surface *> &above);

    void addView(SurfaceView *view);
    void removeView(SurfaceView *view);

Q_SIGNALS:
    void mappedChanged();
    void positionChanged();
    void sizeChanged();
    void damaged(const QRegion &region);
    void belowChanged();
    void aboveChanged();

private Q_SLOTS:
    void handleWindowClosed(Toplevel *toplevel, Deleted *deleted);

protected:
    void frame();
    void discardQuads();
    void discardPixmap();

private:
    Toplevel *m_window;
    QPoint m_position;
    QSize m_size;
    QMatrix4x4 m_surfaceToBufferMatrix;
    QList<Surface *> m_below;
    QList<Surface *> m_above;
    QList<SurfaceView *> m_views;
    bool m_mapped = true;
};

/**
 * The SurfaceView class represents a view to the Surface in a scene graph.
 */
class KWIN_EXPORT SurfaceView
{
public:
    explicit SurfaceView(Surface *surface);
    virtual ~SurfaceView();

    Surface *surface() const;

    virtual void surfaceFrame() {}
    virtual void surfacePixmapInvalidated() {}
    virtual void surfaceQuadsInvalidated() {}

private:
    QPointer<Surface> m_surface;
};

/**
 * The SurfaceTexture class represents a renderer-specific texture with surface's contents.
 */
class KWIN_EXPORT SurfaceTexture
{
public:
    virtual ~SurfaceTexture();
};

/**
 * The SurfacePixmap class manages the client buffer attached to the Surface.
 */
class KWIN_EXPORT SurfacePixmap : public QObject
{
    Q_OBJECT

public:
    explicit SurfacePixmap(SurfaceTexture *platformTexture, QObject *parent = nullptr);

    SurfaceTexture *texture() const;

    bool hasAlphaChannel() const;
    QSize size() const;

    bool isDiscarded() const;
    void markAsDiscarded();

    virtual void create() = 0;
    virtual void update();

    virtual bool isValid() const = 0;

protected:
    QSize m_size;
    bool m_hasAlphaChannel = false;

private:
    QScopedPointer<SurfaceTexture> m_texture;
    bool m_isDiscarded = false;
};

} // namespace KWin
