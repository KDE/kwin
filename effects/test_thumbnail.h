/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Testing of painting a window more than once.

*/

#ifndef KWIN_TEST_THUMBNAIL_H
#define KWIN_TEST_THUMBNAIL_H

#include <effects.h>

namespace KWinInternal
{

class TestThumbnailEffect
    : public Effect
    {
    public:
        TestThumbnailEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void windowActivated( EffectWindow* w );
        virtual void windowDamaged( EffectWindow* w, const QRect& damage );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void windowClosed( EffectWindow* w );
    private:
        QRect thumbnailRect() const;
        EffectWindow* active_window;
    };

} // namespace

#endif
