/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void postPaintScreen();
        void cursorMoved( const QPoint& pos, Qt::MouseButtons buttons );

    protected:
        QRect getThumbnailPosition( EffectWindow* c, int* space ) const;

    private:
        QList< EffectWindow* > mThumbnails;
        QPoint mLastCursorPos;
    };

} // namespace

#endif
