/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Filip Wieladek <wattos@gmail.com>

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

#include "mouseclick_config.h"
// KConfigSkeleton
#include "mouseclickconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QWidget>

K_PLUGIN_FACTORY_WITH_JSON(MouseClickEffectConfigFactory,
                           "mouseclick_config.json",
                           registerPlugin<KWin::MouseClickEffectConfig>();)

namespace KWin
{

MouseClickEffectConfigForm::MouseClickEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MouseClickEffectConfig::MouseClickEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("mouseclick")), parent, args)
{
    m_ui = new MouseClickEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    connect(m_ui->editor, &KShortcutsEditor::keyChange, this, &MouseClickEffectConfig::markAsChanged);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction* a = m_actionCollection->addAction(QStringLiteral("ToggleMouseClick"));
    a->setText(i18n("Toggle Mouse Click Effect"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Asterisk);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Asterisk);

    m_ui->editor->addCollection(m_actionCollection);

    MouseClickConfig::instance(KWIN_CONFIG);
    addConfig(MouseClickConfig::self(), m_ui);
    load();
}

MouseClickEffectConfig::~MouseClickEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void MouseClickEffectConfig::save()
{
    KCModule::save();
    m_ui->editor->save();   // undo() will restore to this state from now on
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mouseclick"));
}

} // namespace

#include "mouseclick_config.moc"
