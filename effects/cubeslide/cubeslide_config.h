/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_CUBESLIDE_CONFIG_H
#define KWIN_CUBESLIDE_CONFIG_H

#include <kcmodule.h>

#include "ui_cubeslide_config.h"


namespace KWin
{

class CubeSlideEffectConfigForm : public QWidget, public Ui::CubeSlideEffectConfigForm
{
    Q_OBJECT
public:
    explicit CubeSlideEffectConfigForm(QWidget* parent);
};

class CubeSlideEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit CubeSlideEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());

public Q_SLOTS:
    void save() override;

private:
    CubeSlideEffectConfigForm* m_ui;
};

} // namespace

#endif
