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
// KConfigSkeleton
#include "flipswitchconfig.h"

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

    addConfig(FlipSwitchConfig::self(), m_ui);

    load();
}

FlipSwitchEffectConfig::~FlipSwitchEffectConfig()
{
}

void FlipSwitchEffectConfig::save()
{
    KCModule::save();

    EffectsHandler::sendReloadMessage("flipswitch");
}


} // namespace

#include "flipswitch_config.moc"
