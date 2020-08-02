/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_COVERSWITCH_CONFIG_H
#define KWIN_COVERSWITCH_CONFIG_H

#include <kcmodule.h>

#include "ui_coverswitch_config.h"


namespace KWin
{

class CoverSwitchEffectConfigForm : public QWidget, public Ui::CoverSwitchEffectConfigForm
{
    Q_OBJECT
public:
    explicit CoverSwitchEffectConfigForm(QWidget* parent);
};

class CoverSwitchEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit CoverSwitchEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());

public Q_SLOTS:
    void save() override;

private:
    CoverSwitchEffectConfigForm* m_ui;
};

} // namespace

#endif
