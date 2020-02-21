/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "lookingglass_config.h"

// KConfigSkeleton
#include "lookingglassconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <KGlobalAccel>
#include <KLocalizedString>
#include <kconfiggroup.h>
#include <KActionCollection>
#include <KAboutData>
#include <KPluginFactory>

#include <QDebug>
#include <QWidget>
#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(LookingGlassEffectConfigFactory,
                           "lookingglass_config.json",
                           registerPlugin<KWin::LookingGlassEffectConfig>();)

namespace KWin
{

LookingGlassEffectConfigForm::LookingGlassEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

LookingGlassEffectConfig::LookingGlassEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("lookingglass")), parent, args)
{
    m_ui = new LookingGlassEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    LookingGlassConfig::instance(KWIN_CONFIG);
    addConfig(LookingGlassConfig::self(), m_ui);
    connect(m_ui->editor, &KShortcutsEditor::keyChange, this, &LookingGlassEffectConfig::markAsChanged);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("LookingGlass"));
    m_actionCollection->setConfigGlobal(true);

    QAction* a;
    a = m_actionCollection->addAction(KStandardAction::ZoomIn);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);

    a = m_actionCollection->addAction(KStandardAction::ZoomOut);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);

    a = m_actionCollection->addAction(KStandardAction::ActualSize);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);

    m_ui->editor->addCollection(m_actionCollection);
}

LookingGlassEffectConfig::~LookingGlassEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void LookingGlassEffectConfig::save()
{
    qDebug() << "Saving config of LookingGlass" ;
    KCModule::save();

    m_ui->editor->save();   // undo() will restore to this state from now on

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("lookingglass"));
}

void LookingGlassEffectConfig::defaults()
{
    m_ui->editor->allDefault();
    KCModule::defaults();
}

} // namespace

#include "lookingglass_config.moc"
