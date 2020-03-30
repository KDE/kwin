/*
 * mouse.h
 *
 * Copyright (c) 1998 Matthias Ettrich <ettrich@kde.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KKWMMOUSECONFIG_H__
#define __KKWMMOUSECONFIG_H__

class KConfig;

#include <kcmodule.h>
#include <KComboBox>
#include <KLocalizedString>

#include "ui_actions.h"
#include "ui_mouse.h"

class KWinOptionsSettings;

class KWinMouseConfigForm : public QWidget, public Ui::KWinMouseConfigForm
{
    Q_OBJECT

public:
    explicit KWinMouseConfigForm(QWidget* parent);
};

class KWinActionsConfigForm : public QWidget, public Ui::KWinActionsConfigForm
{
    Q_OBJECT

public:
    explicit KWinActionsConfigForm(QWidget* parent);
};

class KTitleBarActionsConfig : public KCModule
{
    Q_OBJECT

public:

    KTitleBarActionsConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void save() override;

protected:
    void showEvent(QShowEvent *ev) override;
    void changeEvent(QEvent *ev) override;

private:
    bool standAlone;

    KWinMouseConfigForm *m_ui;
    KWinOptionsSettings *m_settings;

    void createMaximizeButtonTooltips(KComboBox* combo);

private Q_SLOTS:
    void paletteChanged();

};

class KWindowActionsConfig : public KCModule
{
    Q_OBJECT

public:

    KWindowActionsConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void save() override;

protected:
    void showEvent(QShowEvent *ev) override;

private:
    bool standAlone;

    KWinActionsConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};

#endif

