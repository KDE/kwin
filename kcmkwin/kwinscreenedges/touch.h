/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef __TOUCH_H__
#define __TOUCH_H__

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "kwinglobals.h"

#include "ui_touch.h"

class QShowEvent;

namespace KWin
{
enum class BuiltInEffect;

class KWinScreenEdgesConfigForm : public QWidget, public Ui::KWinScreenEdgesConfigForm
{
    Q_OBJECT

public:
    explicit KWinScreenEdgesConfigForm(QWidget* parent);
};

class KWinScreenEdgesConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinScreenEdgesConfig(QWidget* parent, const QVariantList& args);
    ~KWinScreenEdgesConfig();

public Q_SLOTS:
    void save() Q_DECL_OVERRIDE;
    void load() Q_DECL_OVERRIDE;
    void defaults() Q_DECL_OVERRIDE;
protected:
    void showEvent(QShowEvent* e) Q_DECL_OVERRIDE;
private:
    KWinScreenEdgesConfigForm* m_ui;
    KSharedConfigPtr m_config;
    QStringList m_scripts; //list of script IDs ordered in the list they are presented in the menu

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

    bool effectEnabled(const BuiltInEffect& effect, const KConfigGroup& cfg) const;

    void monitorAddItem(const QString& item);
    void monitorItemSetEnabled(int index, bool enabled);
    void monitorInit();
    void monitorLoadAction(ElectricBorder edge, const QString& configName);
    void monitorLoad();
    void monitorSaveAction(int edge, const QString& configName);
    void monitorSave();
    void monitorDefaults();
    void monitorShowEvent();
    void monitorChangeEdge(ElectricBorder border, int index);
    void monitorHideEdge(ElectricBorder border, bool hidden);
    QList<int> monitorCheckEffectHasEdge(int index) const;
};

} // namespace

#endif
