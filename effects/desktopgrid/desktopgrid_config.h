/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DESKTOPGRID_CONFIG_H
#define KWIN_DESKTOPGRID_CONFIG_H

#include <kcmodule.h>

#include "ui_desktopgrid_config.h"
#include "desktopgrid.h"

namespace KWin
{

class DesktopGridEffectConfigForm : public QWidget, public Ui::DesktopGridEffectConfigForm
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfigForm(QWidget* parent);
};

class DesktopGridEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~DesktopGridEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private Q_SLOTS:
    void layoutSelectionChanged();

private:
    DesktopGridEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
