/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

#include "desktopgrideffect.h"
#include "ui_desktopgrid_config.h"

namespace KWin
{

class DesktopGridEffectConfigForm : public QWidget, public Ui::DesktopGridEffectConfigForm
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfigForm(QWidget *parent);
};

class DesktopGridEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~DesktopGridEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private Q_SLOTS:
    void desktopLayoutSelectionChanged();

private:
    DesktopGridEffectConfigForm m_ui;
    KActionCollection *m_actionCollection;
};

} // namespace
