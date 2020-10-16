/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_ZOOM_CONFIG_H
#define KWIN_ZOOM_CONFIG_H

#include <kcmodule.h>

#include "ui_zoom_config.h"


namespace KWin
{

class ZoomEffectConfigForm : public QWidget, public Ui::ZoomEffectConfigForm
{
    Q_OBJECT
public:
    explicit ZoomEffectConfigForm(QWidget* parent = nullptr);
};

class ZoomEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit ZoomEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~ZoomEffectConfig() override;

public Q_SLOTS:
    void save() override;

private:
    ZoomEffectConfigForm* m_ui;
    enum MouseTracking { MouseCentred = 0, MouseProportional = 1, MouseDisabled = 2 };
};

} // namespace

#endif
