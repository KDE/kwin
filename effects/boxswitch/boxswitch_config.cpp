/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "boxswitch_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

BoxSwitchEffectConfigForm::BoxSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

BoxSwitchEffectConfig::BoxSwitchEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new BoxSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->opacitySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->elevateBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->animateBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    load();
}

BoxSwitchEffectConfig::~BoxSwitchEffectConfig()
{
}

void BoxSwitchEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("BoxSwitch");

    int opacity = conf.readEntry("BackgroundOpacity", 25);
    m_ui->opacitySpin->setValue(opacity);

    bool elevate = conf.readEntry("ElevateSelected", true);
    m_ui->elevateBox->setChecked(elevate);

    bool animate = conf.readEntry("AnimateSwitch", false);
    m_ui->animateBox->setChecked(animate);

    emit changed(false);
}

void BoxSwitchEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("BoxSwitch");

    conf.writeEntry("BackgroundOpacity", m_ui->opacitySpin->value());

    conf.writeEntry("ElevateSelected", m_ui->elevateBox->isChecked());

    conf.writeEntry("AnimateSwitch", m_ui->animateBox->isChecked());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("boxswitch");
}

void BoxSwitchEffectConfig::defaults()
{
    m_ui->opacitySpin->setValue(25);
    m_ui->elevateBox->setChecked(true);
    m_ui->animateBox->setChecked(false);
    emit changed(true);
}

} // namespace

#include "boxswitch_config.moc"
