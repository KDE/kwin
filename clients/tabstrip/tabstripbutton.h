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

#ifndef TABSTRIPBUTTON_H
#define TABSTRIPBUTTON_H

#include <kcommondecoration.h>
#include <QSize>
#include <QString>

class TabstripDecoration;

class TabstripButton : public KCommonDecorationButton
    {
    public:
        TabstripButton( ButtonType type, TabstripDecoration *parent, QString tip );
        ~TabstripButton();
        void reset( unsigned long changed );
        QSize sizeHint() const;
        void setActive( bool active );
    private:
        void paintEvent( QPaintEvent *e );
        void leaveEvent( QEvent *e );
        void enterEvent( QEvent *e );
        TabstripDecoration *client;
        ButtonType btype;
        const int SIZE;
        bool active_item;
        bool hovering;
    };

inline void TabstripButton::setActive( bool active )
    {
    active_item = active;
    }

#endif
