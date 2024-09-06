/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursorscene.h"
#include "core/output.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "scene/cursoritem.h"
#include "scene/itemrenderer.h"
#include "scene/rootitem.h"

namespace KWin
{

CursorScene::CursorScene(std::unique_ptr<ItemRenderer> &&renderer)
    : Scene(std::move(renderer))
{
    m_rootItem = std::make_unique<RootItem>(this);
    m_cursorItem = std::make_unique<CursorItem>(m_rootItem.get());
    setGeometry(m_rootItem->boundingRect().toRect());
    connect(m_rootItem.get(), &Item::boundingRectChanged, this, [this]() {
        setGeometry(m_rootItem->boundingRect().toRect());
    });
}

CursorScene::~CursorScene()
{
    // Avoid accessing m_rootItem if the boundingRectChanged signal is emitted.
    disconnect(m_rootItem.get(), nullptr, this, nullptr);
    m_rootItem.reset();
}

static void resetRepaintsHelper(Item *item, SceneDelegate *delegate)
{
    item->resetRepaints(delegate);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, delegate);
    }
}

QRegion CursorScene::prePaint(SceneDelegate *delegate)
{
    resetRepaintsHelper(m_rootItem.get(), delegate);
    m_paintedOutput = delegate->output();
    return m_rootItem->boundingRect().translated(-delegate->viewport().topLeft()).toAlignedRect();
}

void CursorScene::postPaint()
{
}

void CursorScene::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    RenderViewport viewport(QRectF(geometry().topLeft(), QSizeF(renderTarget.size()) / m_paintedOutput->scale()), m_paintedOutput->scale(), renderTarget);
    m_renderer->beginFrame(renderTarget, viewport);
    m_renderer->renderBackground(renderTarget, viewport, region);
    m_renderer->renderItem(renderTarget, viewport, m_rootItem.get(), 0, region, WindowPaintData{});
    m_renderer->endFrame();
}

} // namespace KWin

#include "moc_cursorscene.cpp"
