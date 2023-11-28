/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "magnifier_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "magnifierconfig.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <kconfiggroup.h>

#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>

K_PLUGIN_CLASS(KWin::MagnifierEffectConfig)

namespace KWin
{

MagnifierEffectConfigForm::MagnifierEffectConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

MagnifierEffectConfig::MagnifierEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_ui(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_ui);

    MagnifierConfig::instance(KWIN_CONFIG);
    addConfig(MagnifierConfig::self(), &m_ui);

    connect(m_ui.editor, &KShortcutsEditor::keyChange, this, &MagnifierEffectConfig::markAsChanged);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));

    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("Magnifier"));
    m_actionCollection->setConfigGlobal(true);

    QAction *a;
    a = m_actionCollection->addAction(KStandardAction::ZoomIn);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));

    a = m_actionCollection->addAction(KStandardAction::ZoomOut);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));

    a = m_actionCollection->addAction(KStandardAction::ActualSize);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));

    m_ui.editor->addCollection(m_actionCollection);
}

MagnifierEffectConfig::~MagnifierEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui.editor->undo();
}

void MagnifierEffectConfig::save()
{
    qDebug() << "Saving config of Magnifier";

    m_ui.editor->save(); // undo() will restore to this state from now on
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("magnifier"));
}

void MagnifierEffectConfig::defaults()
{
    m_ui.editor->allDefault();
    KCModule::defaults();
}

} // namespace

#include "magnifier_config.moc"
