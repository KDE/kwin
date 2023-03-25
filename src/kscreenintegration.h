/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/output.h"
#include "core/outputconfiguration.h"

#include <QVector>
#include <optional>

namespace KWin
{
namespace KScreenIntegration
{

QString connectedOutputsHash(const QVector<Output *> &outputs);
std::optional<std::pair<OutputConfiguration, QVector<Output *>>> readOutputConfig(const QVector<Output *> &outputs, const QString &hash);
}
}
