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
#include <config-kwin.h>
#include <kwineffects_interface.h>
#include <QAction>

#include <kconfiggroup.h>
#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(PresentWindowsEffectConfigFactory,
                           "presentwindows_config.json",
                           registerPlugin<KWin::PresentWindowsEffectConfig>();)

namespace KWin
{

PresentWindowsEffectConfigForm::PresentWindowsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule(KAboutData::pluginData(QStringLiteral("presentwindows")), parent, args)
{
    m_ui = new PresentWindowsEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("PresentWindows"));
    m_actionCollection->setConfigGlobal(true);

    QAction* a = m_actionCollection->addAction(QStringLiteral("ExposeAll"));
    a->setText(i18n("Toggle Present Windows (All desktops)"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F10 << Qt::Key_LaunchC);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F10 << Qt::Key_LaunchC);

    QAction* b = m_actionCollection->addAction(QStringLiteral("Expose"));
    b->setText(i18n("Toggle Present Windows (Current desktop)"));
    b->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F9);
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F9);

    QAction* c = m_actionCollection->addAction(QStringLiteral("ExposeClass"));
    c->setText(i18n("Toggle Present Windows (Window class)"));
    c->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(c, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F7);
    KGlobalAccel::self()->setShortcut(c, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F7);

    m_ui->shortcutEditor->addCollection(m_actionCollection);

    connect(m_ui->shortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));

    PresentWindowsConfig::instance(KWIN_CONFIG);
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
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("presentwindows"));
}

void PresentWindowsEffectConfig::defaults()
{
    m_ui->shortcutEditor->allDefault();
    KCModule::defaults();
}

} // namespace

#include "presentwindows_config.moc"
