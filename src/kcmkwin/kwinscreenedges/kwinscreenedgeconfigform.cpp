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
    , ui(std::make_unique<Ui::KWinScreenEdgesConfigUI>())
{
    ui->setupUi(this);

    connect(ui->kcfg_ElectricBorderDelay, qOverload<int>(&QSpinBox::valueChanged), this, &KWinScreenEdgesConfigForm::sanitizeCooldown);

    // Visual feedback of action group conflicts
    connect(ui->kcfg_ElectricBorders, qOverload<int>(&QComboBox::currentIndexChanged), this, &KWinScreenEdgesConfigForm::groupChanged);
    connect(ui->kcfg_ElectricBorderMaximize, &QCheckBox::stateChanged, this, &KWinScreenEdgesConfigForm::groupChanged);
    connect(ui->kcfg_ElectricBorderTiling, &QCheckBox::stateChanged, this, &KWinScreenEdgesConfigForm::groupChanged);

    connect(ui->remainActiveOnFullscreen, &QCheckBox::stateChanged, this, &KWinScreenEdgesConfigForm::onChanged);
    connect(ui->electricBorderCornerRatioSpin, qOverload<int>(&QSpinBox::valueChanged), this, &KWinScreenEdgesConfigForm::onChanged);
    connect(ui->electricBorderCornerRatioSpin, qOverload<int>(&QSpinBox::valueChanged), this, &KWinScreenEdgesConfigForm::updateDefaultIndicators);
}

KWinScreenEdgesConfigForm::~KWinScreenEdgesConfigForm() = default;

void KWinScreenEdgesConfigForm::setElectricBorderCornerRatio(double value)
{
    m_referenceCornerRatio = value;
    ui->electricBorderCornerRatioSpin->setValue(m_referenceCornerRatio * 100.);
}

void KWinScreenEdgesConfigForm::setDefaultElectricBorderCornerRatio(double value)
{
    m_defaultCornerRatio = value;
    updateDefaultIndicators();
}

void KWinScreenEdgesConfigForm::setRemainActiveOnFullscreen(bool remainActive)
{
    m_remainActiveOnFullscreen = remainActive;
    ui->remainActiveOnFullscreen->setChecked(remainActive);
    updateDefaultIndicators();
}

double KWinScreenEdgesConfigForm::electricBorderCornerRatio() const
{
    return ui->electricBorderCornerRatioSpin->value() / 100.;
}

void KWinScreenEdgesConfigForm::setElectricBorderCornerRatioEnabled(bool enable)
{
    ui->electricBorderCornerRatioSpin->setEnabled(enable);
}

void KWinScreenEdgesConfigForm::reload()
{
    ui->electricBorderCornerRatioSpin->setValue(m_referenceCornerRatio * 100.);
    KWinScreenEdge::reload();
}

void KWinScreenEdgesConfigForm::setDefaults()
{
    ui->electricBorderCornerRatioSpin->setValue(m_defaultCornerRatio * 100.);
    ui->remainActiveOnFullscreen->setChecked(false);
    KWinScreenEdge::setDefaults();
}

void KWinScreenEdgesConfigForm::setDefaultsIndicatorsVisible(bool visible)
{
    if (m_defaultIndicatorVisible != visible) {
        m_defaultIndicatorVisible = visible;
        updateDefaultIndicators();
    }
}

bool KWinScreenEdgesConfigForm::remainActiveOnFullscreen() const
{
    return ui->remainActiveOnFullscreen->isChecked();
}

Monitor *KWinScreenEdgesConfigForm::monitor() const
{
    return ui->monitor;
}

bool KWinScreenEdgesConfigForm::isSaveNeeded() const
{
    return m_referenceCornerRatio != electricBorderCornerRatio() || m_remainActiveOnFullscreen != remainActiveOnFullscreen();
}

bool KWinScreenEdgesConfigForm::isDefault() const
{
    return m_defaultCornerRatio == electricBorderCornerRatio() && m_remainActiveOnFullscreen == false;
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

void KWinScreenEdgesConfigForm::updateDefaultIndicators()
{
    ui->electricBorderCornerRatioSpin->setProperty("_kde_highlight_neutral", m_defaultIndicatorVisible && (electricBorderCornerRatio() != m_defaultCornerRatio));
    ui->electricBorderCornerRatioSpin->update();
    ui->remainActiveOnFullscreen->setProperty("_kde_highlight_neutral", m_defaultIndicatorVisible && remainActiveOnFullscreen() == false);
    ui->remainActiveOnFullscreen->update();
}

} // namespace
