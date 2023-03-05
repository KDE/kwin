/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QVector>
#include <memory>

namespace KWin
{

class OutputConfiguration;
class Output;
class OutputMode;

class OutputConfigurationStore
{
public:
    std::pair<OutputConfiguration, QVector<Output *>> queryConfig(const QVector<Output *> &outputs);

private:
    std::pair<OutputConfiguration, QVector<Output *>> generateConfig(const QVector<Output *> &outputs) const;
    std::shared_ptr<OutputMode> chooseMode(Output *output) const;
    double chooseScale(Output *output, OutputMode *mode) const;
    double targetDpi(Output *output) const;
};

}
