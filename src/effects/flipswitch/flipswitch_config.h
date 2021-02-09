/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_FLIPSWITCH_CONFIG_H
#define KWIN_FLIPSWITCH_CONFIG_H

#include <kcmodule.h>

#include "ui_flipswitch_config.h"

class KActionCollection;

namespace KWin
{

class FlipSwitchEffectConfigForm : public QWidget, public Ui::FlipSwitchEffectConfigForm
{
    Q_OBJECT
public:
    explicit FlipSwitchEffectConfigForm(QWidget* parent);
};

class FlipSwitchEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit FlipSwitchEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~FlipSwitchEffectConfig() override;

public Q_SLOTS:
    void save() override;

private:
    FlipSwitchEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
