/*
    main.h

    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    Requires the Qt widget libraries, available at no cost at
    https://www.qt.io

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#ifndef __MAIN_H__
#define __MAIN_H__

#include <QTabWidget>
#include <kcmodule.h>

class KWinOptionsSettings;
class KWinOptionsKDEGlobalsSettings;
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
