/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

class QOpenGLFramebufferObject;

namespace KWin
{

class Deleted;
class InternalWindow;

/**
 * The SurfaceItemInternal class represents an internal surface in the scene.
 */
class KWIN_EXPORT SurfaceItemInternal : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemInternal(InternalWindow *window, Scene *scene, Item *parent = nullptr);

    Window *window() const;

    QVector<QRectF> shape() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged(Window *window, const QRectF &old);
    void handleWindowClosed(Window *original, Deleted *deleted);

protected:
    std::unique_ptr<SurfacePixmap> createPixmap() override;

private:
    Window *m_window;
};

class KWIN_EXPORT SurfacePixmapInternal final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapInternal(SurfaceItemInternal *item, QObject *parent = nullptr);

    QOpenGLFramebufferObject *fbo() const;
    QImage image() const;

    void create() override;
    void update() override;
    bool isValid() const override;

private:
    SurfaceItemInternal *m_item;
    std::shared_ptr<QOpenGLFramebufferObject> m_fbo;
    QImage m_rasterBuffer;
};

} // namespace KWin
