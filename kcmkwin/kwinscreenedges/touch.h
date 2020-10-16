/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __TOUCH_H__
#define __TOUCH_H__

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "kwinglobals.h"

class QShowEvent;

namespace KWin
{
class KWinTouchScreenEdgeConfigForm;
class KWinTouchScreenSettings;
class KWinTouchScreenScriptSettings;
enum class BuiltInEffect;

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
    QStringList m_scripts; //list of script IDs ordered in the list they are presented in the menu
    QHash<QString, KWinTouchScreenScriptSettings*> m_scriptSettings;
    KWinTouchScreenSettings *m_settings;

    enum EffectActions {
        PresentWindowsAll = ELECTRIC_ACTION_COUNT, // Start at the end of built in actions
        PresentWindowsCurrent,
        PresentWindowsClass,
        DesktopGrid,
        Cube,
        Cylinder,
        Sphere,
        TabBox,
        TabBoxAlternative,
        EffectCount
    };

    bool effectEnabled(const BuiltInEffect &effect, const KConfigGroup &cfg) const;

    void monitorInit();
    void monitorLoadSettings();
    void monitorLoadDefaultSettings();
    void monitorSaveSettings();
    void monitorShowEvent();

    static ElectricBorderAction electricBorderActionFromString(const QString &string);
    static QString electricBorderActionToString(int action);
};

} // namespace

#endif
