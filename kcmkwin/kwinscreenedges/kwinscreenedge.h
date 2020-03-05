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

#ifndef __KWINSCREENEDGE_H__
#define __KWINSCREENEDGE_H__

#include <QWidget>

#include "kwinglobals.h"

namespace KWin
{

class Monitor;

class KWinScreenEdge : public QWidget
{
    Q_OBJECT

public:
    explicit KWinScreenEdge(QWidget *parent = nullptr);
    ~KWinScreenEdge() override;

    void monitorHideEdge(ElectricBorder border, bool hidden);

    void monitorAddItem(const QString &item);
    void monitorItemSetEnabled(int index, bool enabled);

    QList<int> monitorCheckEffectHasEdge(int index) const;
    int selectedEdgeItem(ElectricBorder border) const;

    void monitorChangeEdge(ElectricBorder border, int index);
    void monitorChangeEdge(const QList<int> &borderList, int index);

    void monitorChangeDefaultEdge(ElectricBorder border, int index);
    void monitorChangeDefaultEdge(const QList<int> &borderList, int index);

    // revert to reference settings and assess for saveNeeded and default changed
    void reload();
    // reset to default settings and assess for saveNeeded and default changed
    void setDefaults();

private Q_SLOTS:
    void onChanged();
    void createConnection();

Q_SIGNALS:
    void saveNeededChanged(bool isNeeded);
    void defaultChanged(bool isDefault);

private:
    virtual Monitor *monitor() const = 0;

    // internal use, assert if border equals ELECTRIC_COUNT or ElectricNone
    static int electricBorderToMonitorEdge(ElectricBorder border);
    static ElectricBorder monitorEdgeToElectricBorder(int edge);

private:
    QHash<ElectricBorder, int> m_reference; // reference settings
    QHash<ElectricBorder, int> m_default; // default settings
};

} // namespace

#endif
