/*
    SPDX-FileCopyrightText: 1998 Matthias Ettrich <ettrich@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouse.h"
#include "kwinoptions_settings.h"

#include <KWindowSystem>

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

KTitleBarActionsConfig::KTitleBarActionsConfig(KWinOptionsSettings *settings, QWidget *parent)
    : KCModule(parent, KPluginMetaData())
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

KWindowActionsConfig::KWindowActionsConfig(KWinOptionsSettings *settings, QWidget *parent)
    : KCModule(parent, KPluginMetaData())
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

#include "moc_mouse.cpp"
