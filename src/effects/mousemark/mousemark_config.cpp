/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mousemark_config.h"

#include <config-kwin.h>

// KConfigSkeleton
#include "mousemarkconfig.h"
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDebug>
#include <QWidget>

K_PLUGIN_CLASS(KWin::MouseMarkEffectConfig)

namespace KWin
{

MouseMarkEffectConfigForm::MouseMarkEffectConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

MouseMarkEffectConfig::MouseMarkEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_ui(this)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_ui);

    MouseMarkConfig::instance(KWIN_CONFIG);
    addConfig(MouseMarkConfig::self(), &m_ui);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction *a = m_actionCollection->addAction(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear Mouse Marks"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));

    a = m_actionCollection->addAction(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));

    m_ui.editor->addCollection(m_actionCollection);

    connect(m_ui.kcfg_LineWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
        updateSpinBoxSuffix();
    });
}

MouseMarkEffectConfig::~MouseMarkEffectConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    m_ui.editor->undo();
}

void MouseMarkEffectConfig::load()
{
    KCModule::load();

    updateSpinBoxSuffix();
}

void MouseMarkEffectConfig::save()
{
    qDebug() << "Saving config of MouseMark";
    KCModule::save();

    m_actionCollection->writeSettings();
    m_ui.editor->save(); // undo() will restore to this state from now on

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mousemark"));
}

void MouseMarkEffectConfig::updateSpinBoxSuffix()
{
    m_ui.kcfg_LineWidth->setSuffix(i18ncp("Suffix", " pixel", " pixels", m_ui.kcfg_LineWidth->value()));
}

} // namespace

#include "mousemark_config.moc"
