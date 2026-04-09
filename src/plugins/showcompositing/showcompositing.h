/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/offscreenquickview.h"

namespace KWin
{

class ShowCompositingEffect : public Effect
{
    Q_OBJECT

public:
    ShowCompositingEffect();
    ~ShowCompositingEffect() override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    bool blocksDirectScanout() const override;

    static bool supported();

private:
    void removeView(RenderView *view);

    std::unordered_map<RenderView *, std::unique_ptr<OffscreenQuickScene>> m_scene;
};

} // namespace KWin
