/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surface.h"

class QOpenGLFramebufferObject;

namespace KWin
{

class Deleted;
class Toplevel;

/**
 * The SurfaceInternal class represents contents of a window created by kwin.
 */
class KWIN_EXPORT SurfaceInternal : public Surface
{
    Q_OBJECT

public:
    explicit SurfaceInternal(Toplevel *window, QObject *parent = nullptr);

    SurfacePixmap *createPixmap() override;
    QRegion shape() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old);
};

/**
 * The SurfacePixmapInternal class represents a client buffer attached to an internal surface.
 */
class KWIN_EXPORT SurfacePixmapInternal final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapInternal(SurfaceInternal *item, QObject *parent = nullptr);

    QOpenGLFramebufferObject *fbo() const;
    QImage image() const;

    void create() override;
    void update() override;
    bool isValid() const override;

private:
    SurfaceInternal *m_item;
    QSharedPointer<QOpenGLFramebufferObject> m_fbo;
    QImage m_rasterBuffer;
};

} // namespace KWin
