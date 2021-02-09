/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KWindowSystem/private/kwindowshadow_p.h>

namespace KWin
{

class WindowShadowTile final : public KWindowShadowTilePrivate
{
public:
    bool create() override;
    void destroy() override;
};

class WindowShadow final : public KWindowShadowPrivate
{
public:
    bool create() override;
    void destroy() override;
};

} // namespace KWin
