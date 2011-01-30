/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>

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

#include "slidetabs_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kconfiggroup.h>
#include <QWidget>
#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

SlideTabsEffectConfigForm::SlideTabsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

SlideTabsEffectConfig::SlideTabsEffectConfig(QWidget* parent, const QVariantList& args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new SlideTabsEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->grouping, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->switching, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->duration, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    load();
}

void SlideTabsEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("SlideTabs");

    conf.writeEntry("SlideGrouping", m_ui->grouping->isChecked());
    conf.writeEntry("SlideSwitching", m_ui->switching->isChecked());
    conf.writeEntry("SlideDuration", m_ui->duration->value());

    conf.sync();

    KCModule::save();
    emit changed(false);
    EffectsHandler::sendReloadMessage("slidetabs");
}

void SlideTabsEffectConfig::load()
{
    KCModule::load();
    KConfigGroup conf = EffectsHandler::effectConfig("SlideTabs");

    bool switching = conf.readEntry("SlideSwitching", true);
    bool grouping = conf.readEntry("SlideGrouping", true);
    int duration = conf.readEntry("SlideDuration", 500);
    m_ui->switching->setChecked(switching);
    m_ui->grouping->setChecked(grouping);
    m_ui->duration->setValue(duration);
    emit changed(false);
}

void SlideTabsEffectConfig::defaults()
{
    m_ui->grouping->setChecked(true);
    m_ui->switching->setChecked(true);
    m_ui->duration->setValue(500);
    emit changed(true);
}

}

#include "slidetabs_config.moc"
