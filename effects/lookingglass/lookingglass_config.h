/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_LOOKINGGLASS_CONFIG_H
#define KWIN_LOOKINGGLASS_CONFIG_H

#include <kcmodule.h>

#include "ui_lookingglass_config.h"

class KActionCollection;

namespace KWin
{

class LookingGlassEffectConfigForm : public QWidget, public Ui::LookingGlassEffectConfigForm
{
    Q_OBJECT
public:
    explicit LookingGlassEffectConfigForm(QWidget* parent);
};

class LookingGlassEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit LookingGlassEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~LookingGlassEffectConfig() override;

    void save() override;
    void defaults() override;

private:
    LookingGlassEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
