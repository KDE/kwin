/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

#include "ui_magiclamp_config.h"

namespace KWin
{
class MagicLampEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfig(QObject *parent, const KPluginMetaData &data);

public Q_SLOTS:
    void save() override;

private:
    Ui::MagicLampEffectConfigForm m_ui;
};

} // namespace
