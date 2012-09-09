/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Sebastian Sauer <sebsauer@kdab.com>

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

#ifndef KWIN_ZOOM_CONFIG_H
#define KWIN_ZOOM_CONFIG_H

#include <kcmodule.h>

#include "ui_zoom_config.h"


namespace KWin
{

class ZoomEffectConfigForm : public QWidget, public Ui::ZoomEffectConfigForm
{
    Q_OBJECT
public:
    explicit ZoomEffectConfigForm(QWidget* parent = 0);
};

class ZoomEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit ZoomEffectConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    virtual ~ZoomEffectConfig();

public slots:
    virtual void save();

private:
    ZoomEffectConfigForm* m_ui;
    enum MouseTracking { MouseCentred = 0, MouseProportional = 1, MouseDisabled = 2 };
};

} // namespace

#endif
