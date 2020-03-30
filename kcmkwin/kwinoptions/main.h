/*
 * main.h
 *
 * Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 * Requires the Qt widget libraries, available at no cost at
 * https://www.qt.io/
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


#ifndef __MAIN_H__
#define __MAIN_H__

#include <QTabWidget>
#include <kcmodule.h>

class KWinOptionsSettings;
class KFocusConfig;
class KTitleBarActionsConfig;
class KWindowActionsConfig;
class KAdvancedConfig;
class KMovingConfig;

class KWinOptions : public KCModule
{
    Q_OBJECT

public:

    KWinOptions(QWidget *parent, const QVariantList &args);

    void load() override;
    void save() override;
    void defaults() override;
    QString quickHelp() const override;

private:

    QTabWidget   *tab;

    KFocusConfig *mFocus;
    KTitleBarActionsConfig *mTitleBarActions;
    KWindowActionsConfig *mWindowActions;
    KMovingConfig *mMoving;
    KAdvancedConfig *mAdvanced;

    KWinOptionsSettings *mSettings;
};

class KActionsOptions : public KCModule
{
    Q_OBJECT

public:

    KActionsOptions(QWidget *parent, const QVariantList &args);

    void load() override;
    void save() override;
    void defaults() override;

protected Q_SLOTS:

    void moduleChanged(bool state);

private:

    QTabWidget   *tab;

    KTitleBarActionsConfig *mTitleBarActions;
    KWindowActionsConfig *mWindowActions;

    KWinOptionsSettings *mSettings;
};

#endif
