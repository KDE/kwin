/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
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

#include "presentwindows_config.h"
// KConfigSkeleton
#include "presentwindowsconfig.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

PresentWindowsEffectConfigForm::PresentWindowsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new PresentWindowsEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    m_actionCollection->setConfigGroup("PresentWindows");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*) m_actionCollection->addAction("ExposeAll");
    a->setText(i18n("Toggle Present Windows (All desktops)"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));

    KAction* b = (KAction*) m_actionCollection->addAction("Expose");
    b->setText(i18n("Toggle Present Windows (Current desktop)"));
    b->setProperty("isConfigurationAction", true);
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F9));

    KAction* c = (KAction*)m_actionCollection->addAction("ExposeClass");
    c->setText(i18n("Toggle Present Windows (Window class)"));
    c->setProperty("isConfigurationAction", true);
    c->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F7));

    m_ui->shortcutEditor->addCollection(m_actionCollection);

    connect(m_ui->shortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));

    addConfig(PresentWindowsConfig::self(), m_ui);

    load();
}

PresentWindowsEffectConfig::~PresentWindowsEffectConfig()
{
    // If save() is called undoChanges() has no effect
    m_ui->shortcutEditor->undoChanges();
}

void PresentWindowsEffectConfig::save()
{
    KCModule::save();
    m_ui->shortcutEditor->save();
    EffectsHandler::sendReloadMessage("presentwindows");
}

void PresentWindowsEffectConfig::defaults()
{
    m_ui->shortcutEditor->allDefault();
    KCModule::defaults();
}

} // namespace

#include "presentwindows_config.moc"
