/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinscreenedgeconfigform.h"
#include "ui_main.h"

namespace KWin
{

KWinScreenEdgesConfigForm::KWinScreenEdgesConfigForm(QWidget *parent)
    : KWinScreenEdge(parent)
    , ui(new Ui::KWinScreenEdgesConfigUI)
{
    ui->setupUi(this);

    connect(ui->kcfg_ElectricBorderDelay, SIGNAL(valueChanged(int)), this, SLOT(sanitizeCooldown()));

    // Visual feedback of action group conflicts
    connect(ui->kcfg_ElectricBorders, SIGNAL(currentIndexChanged(int)), this, SLOT(groupChanged()));
    connect(ui->kcfg_ElectricBorderMaximize, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));
    connect(ui->kcfg_ElectricBorderTiling, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));

    connect(ui->electricBorderCornerRatioSpin, SIGNAL(valueChanged(int)), this, SLOT(onChanged()));
}

KWinScreenEdgesConfigForm::~KWinScreenEdgesConfigForm()
{
    delete ui;
}

void KWinScreenEdgesConfigForm::setElectricBorderCornerRatio(double value)
{
    m_referenceCornerRatio = value;
    ui->electricBorderCornerRatioSpin->setValue(m_referenceCornerRatio * 100.);
}

void KWinScreenEdgesConfigForm::setDefaultElectricBorderCornerRatio(double value)
{
    m_defaultCornerRatio = value;
}

double KWinScreenEdgesConfigForm::electricBorderCornerRatio() const
{
    return ui->electricBorderCornerRatioSpin->value() / 100.;
}

void KWinScreenEdgesConfigForm::setElectricBorderCornerRatioEnabled(bool enable)
{
    return ui->electricBorderCornerRatioSpin->setEnabled(enable);
}

void KWinScreenEdgesConfigForm::reload()
{
    ui->electricBorderCornerRatioSpin->setValue(m_referenceCornerRatio * 100.);
    KWinScreenEdge::reload();
}

void KWinScreenEdgesConfigForm::setDefaults()
{
    ui->electricBorderCornerRatioSpin->setValue(m_defaultCornerRatio * 100.);
    KWinScreenEdge::setDefaults();
}

Monitor *KWinScreenEdgesConfigForm::monitor() const
{
    return ui->monitor;
}

bool KWinScreenEdgesConfigForm::isSaveNeeded() const
{
    return m_referenceCornerRatio != electricBorderCornerRatio();
}

bool KWinScreenEdgesConfigForm::isDefault() const
{
    return m_defaultCornerRatio == electricBorderCornerRatio();
}

void KWinScreenEdgesConfigForm::sanitizeCooldown()
{
    ui->kcfg_ElectricBorderCooldown->setMinimum(ui->kcfg_ElectricBorderDelay->value() + 50);
}

void KWinScreenEdgesConfigForm::groupChanged()
{
    // Monitor conflicts
    bool hide = false;
    if (ui->kcfg_ElectricBorders->currentIndex() == 2) {
        hide = true;
    }
    monitorHideEdge(ElectricTop, hide);
    monitorHideEdge(ElectricRight, hide);
    monitorHideEdge(ElectricBottom, hide);
    monitorHideEdge(ElectricLeft, hide);
}

} // namespace
