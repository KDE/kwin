/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "kwinglobals.h"

class QShowEvent;

namespace KWin
{
class KWinTouchScreenData;
class KWinTouchScreenEdgeConfigForm;
class KWinTouchScreenScriptSettings;
class KWinTouchScreenEdgeEffectSettings;

class KWinScreenEdgesConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinScreenEdgesConfig(QWidget *parent, const QVariantList &args);
    ~KWinScreenEdgesConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

protected:
    void showEvent(QShowEvent *e) override;

private:
    KWinTouchScreenEdgeConfigForm *m_form;
    KSharedConfigPtr m_config;
    QStringList m_effects; // list of effect IDs ordered in the list they are presented in the menu
    QStringList m_scripts; // list of script IDs ordered in the list they are presented in the menu
    QHash<QString, KWinTouchScreenScriptSettings *> m_scriptSettings;
    QHash<QString, KWinTouchScreenEdgeEffectSettings *> m_effectSettings;
    KWinTouchScreenData *m_data;

    enum EffectActions {
        PresentWindowsAll = ELECTRIC_ACTION_COUNT, // Start at the end of built in actions
        PresentWindowsCurrent,
        PresentWindowsClass,
        TabBox,
        TabBoxAlternative,
        EffectCount
    };

    void monitorInit();
    void monitorLoadSettings();
    void monitorLoadDefaultSettings();
    void monitorSaveSettings();
    void monitorShowEvent();

    static ElectricBorderAction electricBorderActionFromString(const QString &string);
    static QString electricBorderActionToString(int action);
};

} // namespace
