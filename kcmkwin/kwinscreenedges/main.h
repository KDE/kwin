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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "kwinglobals.h"
#include "compositingprefs.h"

#include "ui_main.h"

class QShowEvent;

namespace KWin
{

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

public slots:
    virtual void groupChanged();
    virtual void save();
    virtual void load();
    virtual void defaults();
protected:
    virtual void showEvent(QShowEvent* e);

private:
    KWinScreenEdgesConfigForm* m_ui;
    KSharedConfigPtr m_config;
    CompositingPrefs m_defaultPrefs;

    enum EffectActions {
        PresentWindowsAll = ELECTRIC_ACTION_COUNT, // Start at the end of built in actions
        PresentWindowsCurrent,
        PresentWindowsClass,
        DesktopGrid,
        Cube,
        Cylinder,
        Sphere,
        FlipSwitchAll,
        FlipSwitchCurrent
    };

    bool effectEnabled(const QString& effect, const KConfigGroup& cfg) const;

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
