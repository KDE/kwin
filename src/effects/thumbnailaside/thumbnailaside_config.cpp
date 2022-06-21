/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "thumbnailaside_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "thumbnailasideconfig.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <kconfiggroup.h>

#include <QVBoxLayout>
#include <QWidget>

K_PLUGIN_CLASS(KWin::ThumbnailAsideEffectConfig)

namespace KWin
{

ThumbnailAsideEffectConfigForm::ThumbnailAsideEffectConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

ThumbnailAsideEffectConfig::ThumbnailAsideEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_ui(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_ui);

    connect(m_ui.editor, &KShortcutsEditor::keyChange, this, &ThumbnailAsideEffectConfig::markAsChanged);

    ThumbnailAsideConfig::instance(KWIN_CONFIG);
    addConfig(ThumbnailAsideConfig::self(), this);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("ThumbnailAside"));
    m_actionCollection->setConfigGlobal(true);

    QAction *a = m_actionCollection->addAction(QStringLiteral("ToggleCurrentThumbnail"));
    a->setText(i18n("Toggle Thumbnail for Current Window"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_T));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_T));

    m_ui.editor->addCollection(m_actionCollection);
}

ThumbnailAsideEffectConfig::~ThumbnailAsideEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui.editor->undo();
}

void ThumbnailAsideEffectConfig::save()
{
    KCModule::save();
    m_ui.editor->save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("thumbnailaside"));
}

} // namespace

#include "thumbnailaside_config.moc"
