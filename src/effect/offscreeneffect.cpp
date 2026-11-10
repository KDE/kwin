/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/offscreeneffect.h"
#include "effecthandler.h"
#include "scene/offscreeneffectitem.h"
#include "scene/surfaceitem.h"

namespace KWin
{

class OffscreenEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<OffscreenEffectItem>> windows;
    QMetaObject::Connection windowDeletedConnection;
    RenderGeometry::VertexSnappingMode vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
    std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> applyGeometryCallback;
};

OffscreenEffect::OffscreenEffect(const std::function<void(EffectWindow *window, Item *item, WindowQuadList &quads)> &applyGeometryCallback)
    : d(std::make_unique<OffscreenEffectPrivate>())
{
    d->applyGeometryCallback = applyGeometryCallback;
}

OffscreenEffect::~OffscreenEffect() = default;

bool OffscreenEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OffscreenEffect::redirect(EffectWindow *window)
{
    std::unique_ptr<OffscreenEffectItem> &offscreenItem = d->windows[window];
    if (offscreenItem) {
        return;
    }
    offscreenItem = std::make_unique<OffscreenEffectItem>(window, d->applyGeometryCallback);
    offscreenItem->setVertexSnappingMode(d->vertexSnappingMode);

    if (d->windows.size() == 1) {
        setupConnections();
    }
}

void OffscreenEffect::unredirect(EffectWindow *window)
{
    auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return;
    }

    d->windows.erase(it);
}

void OffscreenEffect::setShader(EffectWindow *window, GLShader *shader)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setShader(shader);
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    d->windowDeletedConnection =
        connect(effects, &EffectsHandler::windowDeleted, this, &OffscreenEffect::handleWindowDeleted);
}

void OffscreenEffect::destroyConnections()
{
    disconnect(d->windowDeletedConnection);

    d->windowDeletedConnection = {};
}

void OffscreenEffect::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    d->vertexSnappingMode = mode;
    for (auto &window : std::as_const(d->windows)) {
        window.second->setVertexSnappingMode(mode);
    }
}

bool OffscreenEffect::blocksDirectScanout() const
{
    return false;
}

class CrossFadeEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<CrossfadeItem>> items;
};

CrossFadeEffect::CrossFadeEffect(QObject *parent)
    : Effect(parent)
    , d(std::make_unique<CrossFadeEffectPrivate>())
{
}

CrossFadeEffect::~CrossFadeEffect() = default;

void CrossFadeEffect::prePaintScreen(ScreenPrePaintData &data)
{
    std::ranges::for_each(d->items | std::views::values, &CrossfadeItem::updateGeometry);
}

void CrossFadeEffect::redirect(EffectWindow *window)
{
    if (d->items.empty()) {
        connect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }

    std::unique_ptr<CrossfadeItem> &offscreenData = d->items[window];
    if (offscreenData) {
        return;
    }
    offscreenData = std::make_unique<CrossfadeItem>(window);
    offscreenData->setZ(100);
}

void CrossFadeEffect::unredirect(EffectWindow *window)
{
    auto it = d->items.find(window);
    if (it == d->items.end()) {
        return;
    }

    d->items.erase(it);
    if (d->items.empty()) {
        disconnect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }
}

void CrossFadeEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void CrossFadeEffect::setAllCrossfadeProgress(double value)
{
    for (const auto &[window, item] : d->items) {
        item->setOpacity(1 - value);
    }
}

void CrossFadeEffect::setCrossfadeProgress(EffectWindow *window, double value)
{
    const auto it = d->items.find(window);
    if (it == d->items.end()) {
        return;
    }
    it->second->setOpacity(1 - value);
}

void CrossFadeEffect::setShader(EffectWindow *window, GLShader *shader)
{
    if (const auto it = d->items.find(window); it != d->items.end()) {
        it->second->setShader(shader);
    }
}

bool CrossFadeEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_offscreeneffect.cpp"
