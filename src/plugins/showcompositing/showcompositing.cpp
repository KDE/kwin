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
    // TODO add a direct toggle in the debug console instead
    effects->scene()->setLayerDebugging(true);
}

ShowCompositingEffect::~ShowCompositingEffect()
{
    effects->scene()->setLayerDebugging(false);
}

bool ShowCompositingEffect::supported()
{
    return true;
}

bool ShowCompositingEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_showcompositing.cpp"
