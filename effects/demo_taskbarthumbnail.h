/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_DEMO_TASKBARTHUMBNAIL_H
#define KWIN_DEMO_TASKBARTHUMBNAIL_H

// Include with base class for effects.
#include <kwineffects.h>


namespace KWin
{

/**
 * Render small thumbnail of window next to it's taskbar entry when the cursor
 *  is above the taskbar entry.
 * Note that this functionality will be replaced in the future so that taskbar
 *  itself can request a thumbnail to be rendered in a give location with a
 *  given size.
 **/
class TaskbarThumbnailEffect
    : public Effect
    {
    public:
        TaskbarThumbnailEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void postPaintScreen();
        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
            Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );

    protected:
        QRect getThumbnailPosition( EffectWindow* c, int* space ) const;

    private:
        QList< EffectWindow* > mThumbnails;
        QPoint mLastCursorPos;
    };

} // namespace

#endif
