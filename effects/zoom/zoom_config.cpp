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
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <KGlobalAccel>
#include <KLocalizedString>
#include <KActionCollection>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(ZoomEffectConfigFactory,
                           "zoom_config.json",
                           registerPlugin<KWin::ZoomEffectConfig>();)

namespace KWin
{

ZoomEffectConfigForm::ZoomEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

ZoomEffectConfig::ZoomEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("zoom")), parent, args)
{
    ZoomConfig::instance(KWIN_CONFIG);
    m_ui = new ZoomEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    addConfig(ZoomConfig::self(), m_ui);

    connect(m_ui->editor, &KShortcutsEditor::keyChange, this, &ZoomEffectConfig::markAsChanged);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("Zoom"));
    actionCollection->setConfigGlobal(true);

    QAction* a;
    a = actionCollection->addAction(KStandardAction::ZoomIn);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Equal);

    a = actionCollection->addAction(KStandardAction::ZoomOut);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Minus);

    a = actionCollection->addAction(KStandardAction::ActualSize);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_0);

    a = actionCollection->addAction(QStringLiteral("MoveZoomLeft"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    a->setText(i18n("Move Left"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Left);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Left);

    a = actionCollection->addAction(QStringLiteral("MoveZoomRight"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    a->setText(i18n("Move Right"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Right);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Right);

    a = actionCollection->addAction(QStringLiteral("MoveZoomUp"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    a->setText(i18n("Move Up"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Up);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Up);

    a = actionCollection->addAction(QStringLiteral("MoveZoomDown"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    a->setText(i18n("Move Down"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Down);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::CTRL + Qt::Key_Down);

    a = actionCollection->addAction(QStringLiteral("MoveMouseToFocus"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Focus"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F5);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F5);

    a = actionCollection->addAction(QStringLiteral("MoveMouseToCenter"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Center"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F6);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_F6);

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
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("zoom"));
}

} // namespace

#include "zoom_config.moc"
