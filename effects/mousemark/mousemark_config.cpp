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

#include "mousemark_config.h"

// KConfigSkeleton
#include "mousemarkconfig.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KLocalizedString>
#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KPluginFactory>

#include <QDebug>
#include <QWidget>

K_PLUGIN_FACTORY_WITH_JSON(MouseMarkEffectConfigFactory,
                           "mousemark_config.json",
                           registerPlugin<KWin::MouseMarkEffectConfig>();)

namespace KWin
{

MouseMarkEffectConfigForm::MouseMarkEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MouseMarkEffectConfig::MouseMarkEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("mousemark")), parent, args)
{
    m_ui = new MouseMarkEffectConfigForm(this);

    m_ui->kcfg_LineWidth->setSuffix(ki18np(" pixel", " pixels"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    addConfig(MouseMarkConfig::self(), m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction* a = m_actionCollection->addAction(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear Mouse Marks"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F11);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F11);

    a = m_actionCollection->addAction(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F12);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F12);

    m_ui->editor->addCollection(m_actionCollection);

    load();
}

MouseMarkEffectConfig::~MouseMarkEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
}

void MouseMarkEffectConfig::save()
{
    qDebug() << "Saving config of MouseMark" ;
    KCModule::save();

    m_actionCollection->writeSettings();
    m_ui->editor->save();   // undo() will restore to this state from now on

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mousemark"));
}

} // namespace

#include "mousemark_config.moc"
