/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "flipswitch_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <KAction>
#include <KActionCollection>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

FlipSwitchEffectConfigForm::FlipSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

FlipSwitchEffectConfig::FlipSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new FlipSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));
    KAction* a = (KAction*)m_actionCollection->addAction("FlipSwitchCurrent");
    a->setText(i18n("Toggle Flip Switch (Current desktop)"));
    a->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
    KAction* b = (KAction*)m_actionCollection->addAction("FlipSwitchAll");
    b->setText(i18n("Toggle Flip Switch (All desktops)"));
    b->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);

    m_actionCollection->setConfigGroup("FlipSwitch");
    m_actionCollection->setConfigGlobal(true);

    m_ui->shortcutEditor->addCollection(m_actionCollection);

    connect(m_ui->durationSpin, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->angleSpin, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->horizontalSlider, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->verticalSlider, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->windowTitleBox, SIGNAL(stateChanged(int)), SLOT(changed()));
    connect(m_ui->shortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));

    load();
}

FlipSwitchEffectConfig::~FlipSwitchEffectConfig()
{
}

void FlipSwitchEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("FlipSwitch");

    m_ui->durationSpin->setValue(conf.readEntry("Duration", 0));
    m_ui->angleSpin->setValue(conf.readEntry("Angle", 30));
    m_ui->horizontalSlider->setValue(conf.readEntry("XPosition", 33));
    // slider bottom is 0, effect bottom is 100
    m_ui->verticalSlider->setValue(100 - conf.readEntry("YPosition", 100));
    m_ui->windowTitleBox->setChecked(conf.readEntry("WindowTitle", true));

    emit changed(false);
}

void FlipSwitchEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("FlipSwitch");

    conf.writeEntry("Duration", m_ui->durationSpin->value());
    conf.writeEntry("Angle", m_ui->angleSpin->value());
    conf.writeEntry("XPosition", m_ui->horizontalSlider->value());
    // slider bottom is 0, effect bottom is 100
    conf.writeEntry("YPosition", 100 - m_ui->verticalSlider->value());
    conf.writeEntry("WindowTitle", m_ui->windowTitleBox->isChecked());

    m_ui->shortcutEditor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("flipswitch");
}

void FlipSwitchEffectConfig::defaults()
{
    m_ui->durationSpin->setValue(0);
    m_ui->angleSpin->setValue(30);
    m_ui->horizontalSlider->setValue(33);
    // slider bottom is 0, effect bottom is 100
    m_ui->verticalSlider->setValue(0);
    m_ui->windowTitleBox->setChecked(true);
    m_ui->shortcutEditor->allDefault();
    emit changed(true);
}


} // namespace

#include "flipswitch_config.moc"
