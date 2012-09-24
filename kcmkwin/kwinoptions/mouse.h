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
#include <klocale.h>

#include "ui_actions.h"
#include "ui_mouse.h"

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

    KTitleBarActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent);
    ~KTitleBarActionsConfig();

    void load();
    void save();
    void defaults();

protected:
    void showEvent(QShowEvent *ev);

public slots:
    void changed() {
        emit KCModule::changed(true);
    }

private:

    KConfig *config;
    bool standAlone;

    KWinMouseConfigForm *m_ui;

    const char* functionTiDbl(int);
    const char* functionTiAc(int);
    const char* functionTiWAc(int);
    const char* functionTiInAc(int);
    const char* functionMax(int);

    void setComboText(KComboBox* combo, const char* text);
    void createMaximizeButtonTooltips(KComboBox* combo);
    const char* fixup(const char* s);

private slots:
    void paletteChanged();

};

class KWindowActionsConfig : public KCModule
{
    Q_OBJECT

public:

    KWindowActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent);
    ~KWindowActionsConfig();

    void load();
    void save();
    void defaults();

protected:
    void showEvent(QShowEvent *ev);

public slots:
    void changed() {
        emit KCModule::changed(true);
    }

private:
    KConfig *config;
    bool standAlone;

    KWinActionsConfigForm *m_ui;

    const char* functionWin(int);
    const char* functionWinWheel(int);
    const char* functionAllKey(int);
    const char* functionAll(int);
    const char* functionAllW(int);

    void setComboText(KComboBox* combo, const char* text);
    const char* fixup(const char* s);
};

#endif

