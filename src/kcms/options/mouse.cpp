/*
    SPDX-FileCopyrightText: 1998 Matthias Ettrich <ettrich@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouse.h"
#include "kwinoptions_settings.h"

#include <KWindowSystem>
#include <QDBusConnection>
#include <QDBusMessage>

KWinMouseConfigForm::KWinMouseConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(parent);
}

KWinActionsConfigForm::KWinActionsConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(parent);
}

KTitleBarActionsConfig::KTitleBarActionsConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent)
    : KCModule(parent, KPluginMetaData())
    , standAlone(_standAlone)
    , m_ui(new KWinMouseConfigForm(widget()))
{
    if (settings) {
        initialize(settings);
    }
}

void KTitleBarActionsConfig::initialize(KWinOptionsSettings *settings)
{
    m_settings = settings;
    addConfig(m_settings, widget());
}

void KTitleBarActionsConfig::save()
{
    KCModule::save();

    if (standAlone) {
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}

KWindowActionsConfig::KWindowActionsConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent)
    : KCModule(parent, KPluginMetaData())
    , standAlone(_standAlone)
    , m_ui(new KWinActionsConfigForm(widget()))
{
    if (settings) {
        initialize(settings);
    }
}

void KWindowActionsConfig::initialize(KWinOptionsSettings *settings)
{
    m_settings = settings;
    addConfig(m_settings, widget());
    m_ui->info_1->setVisible(KWindowSystem::isPlatformX11());
}

void KWindowActionsConfig::save()
{
    KCModule::save();

    if (standAlone) {
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}

#include "moc_mouse.cpp"
