/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "workspacescene_qpainter.h"
// KWin
#include "decorations/decoratedclient.h"
#include "scene/itemrenderer_qpainter.h"
#include "window.h"

// Qt
#include <KDecoration2/Decoration>
#include <QDebug>
#include <QPainter>

#include <cmath>

namespace KWin
{

//****************************************
// SceneQPainter
//****************************************

WorkspaceSceneQPainter::WorkspaceSceneQPainter(QPainterBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererQPainter>())
    , m_backend(backend)
{
}

WorkspaceSceneQPainter::~WorkspaceSceneQPainter()
{
}

std::unique_ptr<Shadow> WorkspaceSceneQPainter::createShadow(Window *window)
{
    return std::make_unique<SceneQPainterShadow>(window);
}

DecorationRenderer *WorkspaceSceneQPainter::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneQPainterDecorationRenderer(impl);
}

//****************************************
// QPainterShadow
//****************************************
SceneQPainterShadow::SceneQPainterShadow(Window *window)
    : Shadow(window)
{
}

SceneQPainterShadow::~SceneQPainterShadow()
{
}

bool SceneQPainterShadow::prepareBackend()
{
    return true;
}

//****************************************
// QPainterDecorationRenderer
//****************************************
SceneQPainterDecorationRenderer::SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : DecorationRenderer(client)
{
}

QImage SceneQPainterDecorationRenderer::image(SceneQPainterDecorationRenderer::DecorationPart part) const
{
    Q_ASSERT(part != DecorationPart::Count);
    return m_images[int(part)];
}

void SceneQPainterDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeImages();
        resetImageSizesDirty();
    }

    auto imageSize = [this](DecorationPart part) {
        return m_images[int(part)].size() / m_images[int(part)].devicePixelRatio();
    };

    const QRect top(QPoint(0, 0), imageSize(DecorationPart::Top));
    const QRect left(QPoint(0, top.height()), imageSize(DecorationPart::Left));
    const QRect right(QPoint(top.width() - imageSize(DecorationPart::Right).width(), top.height()), imageSize(DecorationPart::Right));
    const QRect bottom(QPoint(0, left.y() + left.height()), imageSize(DecorationPart::Bottom));

    const QRect geometry = region.boundingRect();
    auto renderPart = [this](const QRect &rect, const QRect &partRect, int index) {
        if (rect.isEmpty()) {
            return;
        }
        QPainter painter(&m_images[index]);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setWindow(QRect(partRect.topLeft(), partRect.size() * effectiveDevicePixelRatio()));
        painter.setClipRect(rect);
        painter.save();
        // clear existing part
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, Qt::transparent);
        painter.restore();
        client()->decoration()->paint(&painter, rect);
    };

    renderPart(left.intersected(geometry), left, int(DecorationPart::Left));
    renderPart(top.intersected(geometry), top, int(DecorationPart::Top));
    renderPart(right.intersected(geometry), right, int(DecorationPart::Right));
    renderPart(bottom.intersected(geometry), bottom, int(DecorationPart::Bottom));
}

void SceneQPainterDecorationRenderer::resizeImages()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    auto checkAndCreate = [this](int index, const QSizeF &size) {
        auto dpr = effectiveDevicePixelRatio();
        if (m_images[index].size() != size * dpr || m_images[index].devicePixelRatio() != dpr) {
            m_images[index] = QImage(size.toSize() * dpr, QImage::Format_ARGB32_Premultiplied);
            m_images[index].setDevicePixelRatio(dpr);
            m_images[index].fill(Qt::transparent);
        }
    };
    checkAndCreate(int(DecorationPart::Left), left.size());
    checkAndCreate(int(DecorationPart::Right), right.size());
    checkAndCreate(int(DecorationPart::Top), top.size());
    checkAndCreate(int(DecorationPart::Bottom), bottom.size());
}

} // KWin
