/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_CUBE_CONFIG_H
#define KWIN_CUBE_CONFIG_H

#include <kcmodule.h>

#include "ui_cube_config.h"


namespace KWin
{

class CubeEffectConfigForm : public QWidget, public Ui::CubeEffectConfigForm
{
    Q_OBJECT
public:
    explicit CubeEffectConfigForm(QWidget* parent);
};

class CubeEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit CubeEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

public slots:
    virtual void save();

private slots:
    void capsSelectionChanged();
private:
    CubeEffectConfigForm* m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
