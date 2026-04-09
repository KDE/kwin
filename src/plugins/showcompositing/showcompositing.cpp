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
#include "scene/workspacescene.h"

namespace KWin
{

ShowCompositingEffect::ShowCompositingEffect()
{
    connect(effects->scene(), &WorkspaceScene::viewRemoved, this, &ShowCompositingEffect::removeView);
}

ShowCompositingEffect::~ShowCompositingEffect()
{
}

void ShowCompositingEffect::removeView(RenderView *view)
{
    m_scene.erase(view);
}

void ShowCompositingEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);
    auto &scene = m_scene[data.view];
    if (!scene) {
        scene = std::make_unique<OffscreenQuickScene>();
        scene->loadFromModule(QStringLiteral("org.kde.kwin.showcompositing"), QStringLiteral("Main"), {});
    }
    const auto rect = data.view->viewport();
    scene->setGeometry(QRect(rect.x() + rect.width() - 300, rect.y(), 300, 150));
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
