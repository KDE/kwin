/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#ifndef KWIN_TABBOX_LAYOUTCONFIG_H
#define KWIN_TABBOX_LAYOUTCONFIG_H

#include <QWidget>

namespace KWin
{

namespace TabBox
{
class TabBoxConfig;

class LayoutConfigPrivate;

class LayoutConfig : public QWidget
    {
    Q_OBJECT

    public:
        LayoutConfig( QWidget* parent = NULL );
        ~LayoutConfig();

        void setConfig( const TabBoxConfig& config );
        TabBoxConfig& config() const;

    private slots:
        void changed();

    private:
        LayoutConfigPrivate* d;
    };

} // namespace TabBox
} // namespace KWin

#endif // KWIN_TABBOX_LAYOUTCONFIG_H
