/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "magiclamp_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

MagicLampEffectConfigForm::MagicLampEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MagicLampEffectConfig::MagicLampEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new MagicLampEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->animationDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    load();
}

void MagicLampEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("MagicLamp");

    int duration       = conf.readEntry("AnimationDuration", 0);
    m_ui->animationDurationSpin->setValue(duration);
    m_ui->animationDurationSpin->setSuffix(ki18np(" millisecond", " milliseconds"));
    emit changed(false);
}

void MagicLampEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("MagicLamp");

    conf.writeEntry("AnimationDuration", m_ui->animationDurationSpin->value());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("magiclamp");
}

void MagicLampEffectConfig::defaults()
{
    m_ui->animationDurationSpin->setValue(0);
    emit changed(true);
}
} // namespace

#include "magiclamp_config.moc"
