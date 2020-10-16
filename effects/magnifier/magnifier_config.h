/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_MAGNIFIER_CONFIG_H
#define KWIN_MAGNIFIER_CONFIG_H

#include <kcmodule.h>

#include "ui_magnifier_config.h"

class KActionCollection;

namespace KWin
{

class MagnifierEffectConfigForm : public QWidget, public Ui::MagnifierEffectConfigForm
{
    Q_OBJECT
public:
    explicit MagnifierEffectConfigForm(QWidget* parent);
};

class MagnifierEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MagnifierEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~MagnifierEffectConfig() override;

    void save() override;
    void defaults() override;

private:
    MagnifierEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
