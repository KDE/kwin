/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Sebastian Sauer <sebsauer@kdab.com>

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

#include "zoom_config.h"
// KConfigSkeleton
#include "zoomconfig.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ZoomEffectConfigForm::ZoomEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

ZoomEffectConfig::ZoomEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new ZoomEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    addConfig(ZoomConfig::self(), m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, KComponentData("kwin"));
    actionCollection->setConfigGroup("Zoom");
    actionCollection->setConfigGlobal(true);

    KAction* a;
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomIn));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Equal));

    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomOut));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));

    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ActualSize));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomLeft"));
    a->setIcon(KIcon("go-previous"));
    a->setText(i18n("Move Left"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomRight"));
    a->setIcon(KIcon("go-next"));
    a->setText(i18n("Move Right"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomUp"));
    a->setIcon(KIcon("go-up"));
    a->setText(i18n("Move Up"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomDown"));
    a->setIcon(KIcon("go-down"));
    a->setText(i18n("Move Down"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));

    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToFocus"));
    a->setIcon(KIcon("view-restore"));
    a->setText(i18n("Move Mouse to Focus"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F5));

    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToCenter"));
    a->setIcon(KIcon("view-restore"));
    a->setText(i18n("Move Mouse to Center"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F6));

    m_ui->editor->addCollection(actionCollection);
    load();
}

ZoomEffectConfig::~ZoomEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void ZoomEffectConfig::save()
{
    m_ui->editor->save(); // undo() will restore to this state from now on
    KCModule::save();
    EffectsHandler::sendReloadMessage("zoom");
}

} // namespace

#include "zoom_config.moc"
