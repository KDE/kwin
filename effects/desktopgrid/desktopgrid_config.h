/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_DESKTOPGRID_CONFIG_H
#define KWIN_DESKTOPGRID_CONFIG_H

#include <kcmodule.h>

#include "ui_desktopgrid_config.h"
#include "desktopgrid.h"

namespace KWin
{

class DesktopGridEffectConfigForm : public QWidget, public Ui::DesktopGridEffectConfigForm
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfigForm(QWidget* parent);
};

class DesktopGridEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit DesktopGridEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    ~DesktopGridEffectConfig();

public slots:
    virtual void save();
    virtual void load();
    virtual void defaults();

private slots:
    void layoutSelectionChanged();

private:
    DesktopGridEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
