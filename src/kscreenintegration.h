/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/backendoutput.h"
#include "core/outputconfiguration.h"

#include <QList>
#include <optional>

namespace KWin
{
namespace KScreenIntegration
{

/**
 * This function reads the output configuration from the Xorg session. The main usecase is to preserve
 * existing configuration when a user switches from the Xorg to the Wayland session for the first time.
 * When the Xorg session is dropped, this function can be dropped some time later.
 */
std::optional<OutputConfiguration> readOutputConfig(const QList<BackendOutput *> &outputs, bool isLidClosed);
}
}
