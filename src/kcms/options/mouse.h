/*
    mouse.h

    SPDX-FileCopyrightText: 1998 Matthias Ettrich <ettrich@kde.org>


    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class KConfig;

#include <KCModule>
#include <KLocalizedString>

#include "ui_actions.h"
#include "ui_mouse.h"

class KWinOptionsSettings;

class KWinMouseConfigForm : public QWidget, public Ui::KWinMouseConfigForm
{
    Q_OBJECT

public:
    explicit KWinMouseConfigForm(QWidget *parent);
};

class KWinActionsConfigForm : public QWidget, public Ui::KWinActionsConfigForm
{
    Q_OBJECT

public:
    explicit KWinActionsConfigForm(QWidget *parent);
};

class KTitleBarActionsConfig : public KCModule
{
    Q_OBJECT

public:
    KTitleBarActionsConfig(KWinOptionsSettings *settings, QWidget *parent);

protected:
    void initialize(KWinOptionsSettings *settings);

private:
    KWinMouseConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};

class KWindowActionsConfig : public KCModule
{
    Q_OBJECT

public:
    KWindowActionsConfig(KWinOptionsSettings *settings, QWidget *parent);

    bool isDefaults() const;
    bool isSaveNeeded() const;

protected:
    void initialize(KWinOptionsSettings *settings);

private:
    KWinActionsConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};
