/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2020 Cyril Rossi <cyril.rossi@enioka.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "kwinglobals.h"

class QShowEvent;

namespace KWin
{
class KWinScreenEdgesConfigForm;
class KWinScreenEdgeSettings;
class KWinScreenEdgeScriptSettings;
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
    KWinScreenEdgesConfigForm *m_form;
    KSharedConfigPtr m_config;
    QStringList m_scripts; //list of script IDs ordered in the list they are presented in the menu
    QHash<QString, KWinScreenEdgeScriptSettings*> m_scriptSettings;
    KWinScreenEdgeSettings *m_settings;

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
