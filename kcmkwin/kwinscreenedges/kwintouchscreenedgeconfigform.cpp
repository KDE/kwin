/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintouchscreenedgeconfigform.h"
#include "ui_touch.h"

namespace KWin
{

KWinTouchScreenEdgeConfigForm::KWinTouchScreenEdgeConfigForm(QWidget *parent)
    : KWinScreenEdge(parent)
    , ui(new Ui::KWinTouchScreenConfigUi)
{
    ui->setupUi(this);
}

KWinTouchScreenEdgeConfigForm::~KWinTouchScreenEdgeConfigForm()
{
    delete ui;
}

Monitor *KWinTouchScreenEdgeConfigForm::monitor() const
{
    return ui->monitor;
}

} // namespace
