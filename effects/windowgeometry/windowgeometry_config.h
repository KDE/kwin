/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Thomas Lübking <thomas.luebking@web.de>

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

#ifndef WINDOWGEOMETRY_CONFIG_H
#define WINDOWGEOMETRY_CONFIG_H

#include <kcmodule.h>

#include "ui_windowgeometry_config.h"


namespace KWin
{

class WindowGeometryConfigForm : public QWidget, public Ui::WindowGeometryConfigForm
{
    Q_OBJECT
public:
    explicit WindowGeometryConfigForm(QWidget* parent);
};

class WindowGeometryConfig : public KCModule
{
    Q_OBJECT
public:
    explicit WindowGeometryConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());
    ~WindowGeometryConfig();

public slots:
    void save();
    void defaults();

private:
    WindowGeometryConfigForm* myUi;
    KActionCollection* myActionCollection;
};

} // namespace

#endif
