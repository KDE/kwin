/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SHOWFPS_CONFIG_H
#define KWIN_SHOWFPS_CONFIG_H

#include <kcmodule.h>

#include "ui_showfps_config.h"

namespace KWin
{

class ShowFpsEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit ShowFpsEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~ShowFpsEffectConfig() override;

public Q_SLOTS:
    void save() override;

private:
    Ui::ShowFpsEffectConfigForm *m_ui;
};

} // namespace

#endif
