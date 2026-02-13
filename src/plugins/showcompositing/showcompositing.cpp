/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showcompositing.h"
#include "core/output.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"

namespace KWin
{

ShowCompositingEffect::ShowCompositingEffect()
{
}

ShowCompositingEffect::~ShowCompositingEffect() = default;

void ShowCompositingEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);
    if (!m_scene) {
        m_scene = std::make_unique<OffscreenQuickScene>();
        m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showcompositing"), QStringLiteral("Main"), {});
    }
}

void ShowCompositingEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    const auto rect = viewport.renderRect();
    m_scene->setGeometry(QRect(rect.x() + rect.width() - 300, rect.y(), 300, 150));
    effects->renderOffscreenQuickView(renderTarget, viewport, m_scene.get());
}

bool ShowCompositingEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool ShowCompositingEffect::blocksDirectScanout() const
{
    // this is intentionally wrong, as we want direct scanout to change the image
    // with this effect
    return false;
}

} // namespace KWin

#include "moc_showcompositing.cpp"
