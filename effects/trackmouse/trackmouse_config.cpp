/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

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

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kaction.h>
#include <KActionCollection>
#include <KShortcutsEditor>

#include <QVBoxLayout>
#include <QLabel>

#include "trackmouse_config.h"

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

TrackMouseEffectConfigForm::TrackMouseEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

TrackMouseEffectConfig::TrackMouseEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{

    m_ui = new TrackMouseEffectConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);
    connect(m_ui->alt, SIGNAL(clicked(bool)), SLOT(changed()));
    connect(m_ui->control, SIGNAL(clicked(bool)), SLOT(changed()));
    connect(m_ui->meta, SIGNAL(clicked(bool)), SLOT(changed()));
    connect(m_ui->shift, SIGNAL(clicked(bool)), SLOT(changed()));
    connect(m_ui->modifierRadio, SIGNAL(clicked(bool)), SLOT(changed()));
    connect(m_ui->shortcutRadio, SIGNAL(clicked(bool)), SLOT(changed()));

    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));
    m_actionCollection->setConfigGroup("TrackMouse");
    m_actionCollection->setConfigGlobal(true);

    KAction *a = static_cast< KAction* >(m_actionCollection->addAction("TrackMouse"));
    a->setText(i18n("Track mouse"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut());
    connect(m_ui->shortcut, SIGNAL(keySequenceChanged(const QKeySequence&)),
                            SLOT(shortcutChanged(const QKeySequence&)));

    load();
}

TrackMouseEffectConfig::~TrackMouseEffectConfig()
{
}

void TrackMouseEffectConfig::load()
{
    KCModule::load();
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("TrackMouse")))
        m_ui->shortcut->setKeySequence(a->globalShortcut().primary());
    KConfigGroup conf = EffectsHandler::effectConfig("TrackMouse");
    m_ui->meta->setChecked(conf.readEntry("Meta", true));
    m_ui->control->setChecked(conf.readEntry("Control", true));
    m_ui->alt->setChecked(conf.readEntry("Alt", false));
    m_ui->shift->setChecked(conf.readEntry("Shift", false));
    const bool modifiers = m_ui->shift->isChecked() || m_ui->alt->isChecked() ||
                           m_ui->control->isChecked() || m_ui->meta->isChecked();
    m_ui->modifierRadio->setChecked(modifiers);
    m_ui->shortcutRadio->setChecked(!modifiers);
    emit changed(false);
}

void TrackMouseEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("TrackMouse");
    conf.writeEntry("Shift", m_ui->modifierRadio->isChecked() && m_ui->shift->isChecked());
    conf.writeEntry("Alt", m_ui->modifierRadio->isChecked() && m_ui->alt->isChecked());
    conf.writeEntry("Control", m_ui->modifierRadio->isChecked() && m_ui->control->isChecked());
    conf.writeEntry("Meta", m_ui->modifierRadio->isChecked() && m_ui->meta->isChecked());
    m_actionCollection->writeSettings();
    conf.sync();
    emit changed(false);
    EffectsHandler::sendReloadMessage("trackmouse");
}

void TrackMouseEffectConfig::defaults()
{
    m_ui->shortcut->clearKeySequence();
    m_ui->meta->setChecked(true);
    m_ui->control->setChecked(true);
    m_ui->alt->setChecked(false);
    m_ui->shift->setChecked(false);
    emit changed(true);
}

void TrackMouseEffectConfig::shortcutChanged(const QKeySequence &seq)
{
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("TrackMouse")))
        a->setGlobalShortcut(KShortcut(seq), KAction::ActiveShortcut, KAction::NoAutoloading);
//     m_actionCollection->writeSettings();
    emit changed(true);
}


} // namespace

#include "trackmouse_config.moc"
