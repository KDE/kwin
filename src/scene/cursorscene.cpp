/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursorscene.h"
#include "core/rendertarget.h"
#include "scene/cursoritem.h"
#include "scene/itemrenderer.h"

namespace KWin
{

CursorScene::CursorScene(std::unique_ptr<ItemRenderer> &&renderer)
    : Scene(std::move(renderer))
{
}

CursorScene::~CursorScene()
{
    // Avoid accessing m_rootItem if the boundingRectChanged signal is emitted.
    disconnect(m_rootItem.get(), nullptr, this, nullptr);
    m_rootItem.reset();
}

void CursorScene::initialize()
{
    m_rootItem = std::make_unique<CursorItem>(this);
    setGeometry(m_rootItem->boundingRect().toRect());
    connect(m_rootItem.get(), &Item::boundingRectChanged, this, [this]() {
        setGeometry(m_rootItem->boundingRect().toRect());
    });
}

static void resetRepaintsHelper(Item *item, SceneDelegate *delegate)
{
    item->resetRepaints(delegate);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, delegate);
    }
}

void CursorScene::prePaint(SceneDelegate *delegate)
{
    resetRepaintsHelper(m_rootItem.get(), delegate);
}

void CursorScene::postPaint()
{
}

void CursorScene::paint(RenderTarget *renderTarget, const QRegion &region)
{
    m_renderer->setRenderTargetRect(QRect(QPoint(0, 0), renderTarget->size() / renderTarget->devicePixelRatio()));
    m_renderer->setRenderTargetScale(renderTarget->devicePixelRatio());

    m_renderer->beginFrame(renderTarget);
    m_renderer->renderBackground(region);
    m_renderer->renderItem(m_rootItem.get(), 0, region, WindowPaintData(m_renderer->renderTargetProjectionMatrix()));
    m_renderer->endFrame();
}

} // namespace KWin
