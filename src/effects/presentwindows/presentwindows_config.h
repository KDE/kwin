/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_PRESENTWINDOWS_CONFIG_H
#define KWIN_PRESENTWINDOWS_CONFIG_H

#include <kcmodule.h>

#include "ui_presentwindows_config.h"

namespace KWin
{

class PresentWindowsEffectConfigForm : public QWidget, public Ui::PresentWindowsEffectConfigForm
{
    Q_OBJECT
public:
    explicit PresentWindowsEffectConfigForm(QWidget* parent);
};

class PresentWindowsEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit PresentWindowsEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~PresentWindowsEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    PresentWindowsEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
