/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#ifndef KWIN_LOOKINGGLASS_H
#define KWIN_LOOKINGGLASS_H

#include <kwinshadereffect.h>

class KActionCollection;

namespace KWin
{

/**
 * Enhanced magnifier
 **/
class LookingGlassEffect : public QObject, public ShaderEffect
    {
    Q_OBJECT
    public:
        LookingGlassEffect();
        virtual ~LookingGlassEffect();

        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
            Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );

    public slots:
        void toggle();
        void zoomIn();
        void zoomOut();

    private:
        double zoom;
        double target_zoom;
        int radius;
        int initialradius;
        KActionCollection* actionCollection;
    };

} // namespace

#endif
