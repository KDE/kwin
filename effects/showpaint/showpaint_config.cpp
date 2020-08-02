/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showpaint_config.h"

#include <KAboutData>
#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KShortcutsEditor>

#include <QAction>

K_PLUGIN_FACTORY_WITH_JSON(ShowPaintEffectConfigFactory,
                           "showpaint_config.json",
                           registerPlugin<KWin::ShowPaintEffectConfig>();)

namespace KWin
{

ShowPaintEffectConfig::ShowPaintEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(QStringLiteral("showpaint")), parent, args)
    , m_ui(new Ui::ShowPaintEffectConfig)
{
    m_ui->setupUi(this);

    auto *actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("ShowPaint"));
    actionCollection->setConfigGlobal(true);

    QAction *toggleAction = actionCollection->addAction(QStringLiteral("Toggle"));
    toggleAction->setText(i18n("Toggle Show Paint"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {});
    KGlobalAccel::self()->setShortcut(toggleAction, {});

    m_ui->shortcutsEditor->addCollection(actionCollection);

    connect(m_ui->shortcutsEditor, &KShortcutsEditor::keyChange,
            this, &ShowPaintEffectConfig::markAsChanged);

    load();
}

ShowPaintEffectConfig::~ShowPaintEffectConfig()
{
    // If save() is called, undoChanges() has no effect.
    m_ui->shortcutsEditor->undoChanges();

    delete m_ui;
}

void ShowPaintEffectConfig::save()
{
    KCModule::save();
    m_ui->shortcutsEditor->save();
}

void ShowPaintEffectConfig::defaults()
{
    m_ui->shortcutsEditor->allDefault();
    KCModule::defaults();
}

} // namespace KWin

#include "showpaint_config.moc"
