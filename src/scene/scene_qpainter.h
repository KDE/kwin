/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "qpainterbackend.h"

#include "scene/decorationitem.h"
#include "scene/scene.h"
#include "shadow.h"

namespace KWin
{

class KWIN_EXPORT SceneQPainter : public Scene
{
    Q_OBJECT

public:
    explicit SceneQPainter(QPainterBackend *backend);
    ~SceneQPainter() override;

    void paint(RenderTarget *renderTarget, const QRegion &region) override;
    Shadow *createShadow(Window *window) override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void render(Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    bool animationsSupported() const override
    {
        return false;
    }

    QPainter *scenePainter() const override;

    QPainterBackend *backend() const
    {
        return m_backend;
    }

protected:
    void paintBackground(const QRegion &region) override;
    void paintOffscreenQuickView(OffscreenQuickView *w) override;

private:
    void renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const;
    void renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const;
    void renderItem(QPainter *painter, Item *item) const;

    QPainterBackend *m_backend;
    std::unique_ptr<QPainter> m_painter;
};

class SceneQPainterShadow : public Shadow
{
public:
    SceneQPainterShadow(Window *window);
    ~SceneQPainterShadow() override;

protected:
    bool prepareBackend() override;
};

class SceneQPainterDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client);

    void render(const QRegion &region) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

inline QPainter *SceneQPainter::scenePainter() const
{
    return m_painter.get();
}

} // KWin
