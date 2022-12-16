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

class MagicLampEffectConfigForm : public QWidget, public Ui::MagicLampEffectConfigForm
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfigForm(QWidget *parent);
};

class MagicLampEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MagicLampEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());

public Q_SLOTS:
    void save() override;

private:
    MagicLampEffectConfigForm m_ui;
};

} // namespace
