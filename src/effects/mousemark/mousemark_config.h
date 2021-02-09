/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_MOUSEMARK_CONFIG_H
#define KWIN_MOUSEMARK_CONFIG_H

#include <kcmodule.h>

#include "ui_mousemark_config.h"

class KActionCollection;

namespace KWin
{

class MouseMarkEffectConfigForm : public QWidget, public Ui::MouseMarkEffectConfigForm
{
    Q_OBJECT
public:
    explicit MouseMarkEffectConfigForm(QWidget* parent);
};

class MouseMarkEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit MouseMarkEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~MouseMarkEffectConfig() override;

    void save() override;

private:
    MouseMarkEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
