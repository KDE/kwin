/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "coverswitch_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

CoverSwitchEffectConfigForm::CoverSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

CoverSwitchEffectConfig::CoverSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new CoverSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->checkAnimateSwitch, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkAnimateStart, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkAnimateStop, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkReflection, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->colorFront, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->colorRear, SIGNAL(changed(QColor)), this, SLOT(changed()));
    connect(m_ui->checkWindowTitle, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkThumbnails, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkDynamicThumbnails, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinThumbnailWindows, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinDuration, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->zPositionSlider, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    connect(m_ui->checkThumbnails, SIGNAL(stateChanged(int)), this, SLOT(thumbnailsChanged()));
    connect(m_ui->checkDynamicThumbnails, SIGNAL(stateChanged(int)), this, SLOT(thumbnailsChanged()));

    load();
}

void CoverSwitchEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("CoverSwitch");

    m_ui->spinDuration->setValue(conf.readEntry("Duration", 0));
    m_ui->spinDuration->setSuffix(ki18np(" millisecond", " milliseconds"));
    m_ui->checkAnimateSwitch->setChecked(conf.readEntry("AnimateSwitch", true));
    m_ui->checkAnimateStart->setChecked(conf.readEntry("AnimateStart", true));
    m_ui->checkAnimateStop->setChecked(conf.readEntry("AnimateStop", true));
    m_ui->checkReflection->setChecked(conf.readEntry("Reflection", true));
    m_ui->colorFront->setColor(conf.readEntry("MirrorFrontColor", QColor(0, 0, 0)));
    m_ui->colorRear->setColor(conf.readEntry("MirrorRearColor", QColor(0, 0, 0)));
    m_ui->checkWindowTitle->setChecked(conf.readEntry("WindowTitle", true));
    m_ui->checkThumbnails->setChecked(conf.readEntry("Thumbnails", true));
    m_ui->checkDynamicThumbnails->setChecked(conf.readEntry("DynamicThumbnails", true));
    m_ui->spinThumbnailWindows->setValue(conf.readEntry("ThumbnailWindows", 8));
    m_ui->zPositionSlider->setValue(conf.readEntry("ZPosition", 900));

    thumbnailsChanged();

    emit changed(false);
}

void CoverSwitchEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("CoverSwitch");

    conf.writeEntry("Duration", m_ui->spinDuration->value());
    conf.writeEntry("AnimateSwitch", m_ui->checkAnimateSwitch->isChecked());
    conf.writeEntry("AnimateStart", m_ui->checkAnimateStart->isChecked());
    conf.writeEntry("AnimateStop", m_ui->checkAnimateStop->isChecked());
    conf.writeEntry("Reflection", m_ui->checkReflection->isChecked());
    conf.writeEntry("MirrorFrontColor", m_ui->colorFront->color());
    conf.writeEntry("MirrorRearColor", m_ui->colorRear->color());
    conf.writeEntry("WindowTitle", m_ui->checkWindowTitle->isChecked());
    conf.writeEntry("Thumbnails", m_ui->checkThumbnails->isChecked());
    conf.writeEntry("DynamicThumbnails", m_ui->checkDynamicThumbnails->isChecked());
    conf.writeEntry("ThumbnailWindows", m_ui->spinThumbnailWindows->value());
    conf.writeEntry("ZPosition", m_ui->zPositionSlider->value());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("coverswitch");
}

void CoverSwitchEffectConfig::defaults()
{
    m_ui->spinDuration->setValue(0);
    m_ui->checkAnimateSwitch->setCheckState(Qt::Checked);
    m_ui->checkAnimateStart->setCheckState(Qt::Checked);
    m_ui->checkAnimateStop->setCheckState(Qt::Checked);
    m_ui->checkReflection->setCheckState(Qt::Checked);
    m_ui->colorFront->setColor(QColor(0, 0, 0));
    m_ui->colorRear->setColor(QColor(0, 0, 0));
    m_ui->checkWindowTitle->setCheckState(Qt::Checked);
    m_ui->checkThumbnails->setCheckState(Qt::Checked);
    m_ui->checkDynamicThumbnails->setCheckState(Qt::Checked);
    m_ui->spinThumbnailWindows->setValue(8);
    m_ui->zPositionSlider->setValue(900);
    emit changed(true);
}

void CoverSwitchEffectConfig::thumbnailsChanged()
{
    bool enabled = m_ui->checkThumbnails->isChecked() && m_ui->checkDynamicThumbnails->isChecked();
    m_ui->checkDynamicThumbnails->setEnabled(m_ui->checkThumbnails->isChecked());
    m_ui->spinThumbnailWindows->setEnabled(enabled);
    m_ui->labelThumbnailWindows->setEnabled(enabled);
}

} // namespace

#include "coverswitch_config.moc"
