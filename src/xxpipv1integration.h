/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class XXPipV1Interface;

class XXPipV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit XXPipV1Integration(QObject *parent = nullptr);

private:
    void registerPipV1Surface(XXPipV1Interface *pip);
    void createPipV1Window(XXPipV1Interface *pip);
};

} // namespace KWin
