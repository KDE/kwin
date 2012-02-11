/********************************************************************
Tabstrip KWin window decoration
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef TABSTRIPFACTORY_H
#define TABSTRIPFACTORY_H

#include <kdecorationfactory.h>
#include <kdecoration.h>
#include <kdecorationbridge.h>

class TabstripFactory : public KDecorationFactoryUnstable
    {
    public:
        TabstripFactory();
        ~TabstripFactory();
        KDecoration *createDecoration( KDecorationBridge *bridge );
        bool supports( Ability ability ) const;
        bool reset( unsigned long changed );
        QList< KDecorationDefines::BorderSize > borderSizes() const;
        static Qt::AlignmentFlag titleAlign();
        static bool showIcon();
        static QColor &outlineColor();
    private:
        bool readConfig();
        bool initialized;
        static Qt::AlignmentFlag titlealign;
        static bool show_icon;
        static QColor outline_color;
    };

inline Qt::AlignmentFlag TabstripFactory::titleAlign()
    {
    return titlealign;
    }

inline bool TabstripFactory::showIcon()
    {
    return show_icon;
    }
inline QColor &TabstripFactory::outlineColor()
    {
    return outline_color;
    }

#endif
