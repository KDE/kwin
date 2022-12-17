/*
    SPDX-FileCopyrightText: 1998 Matthias Ettrich <ettrich@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouse.h"

#include <QtDBus>

#include <cstdlib>

#include "kwinoptions_settings.h"

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
    : KCModule(parent)
    , standAlone(_standAlone)
    , m_ui(new KWinMouseConfigForm(this))
{
    if (settings) {
        initialize(settings);
    }
}

void KTitleBarActionsConfig::initialize(KWinOptionsSettings *settings)
{
    m_settings = settings;
    addConfig(m_settings, this);
}

void KTitleBarActionsConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        // Workaround KCModule::showEvent() calling load(), see bug 163817
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KTitleBarActionsConfig::changeEvent(QEvent *ev)
{
    ev->accept();
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

bool KTitleBarActionsConfig::isDefaults() const
{
    return managedWidgetDefaultState();
}

bool KTitleBarActionsConfig::isSaveNeeded() const
{
    return managedWidgetChangeState();
}

KWindowActionsConfig::KWindowActionsConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent)
    : KCModule(parent)
    , standAlone(_standAlone)
    , m_ui(new KWinActionsConfigForm(this))
{
    if (settings) {
        initialize(settings);
    }
}

void KWindowActionsConfig::initialize(KWinOptionsSettings *settings)
{
    m_settings = settings;
    addConfig(m_settings, this);
}

void KWindowActionsConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
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

bool KWindowActionsConfig::isDefaults() const
{
    return managedWidgetDefaultState();
}

bool KWindowActionsConfig::isSaveNeeded() const
{
    return managedWidgetChangeState();
}
