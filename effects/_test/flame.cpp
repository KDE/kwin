/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "flame.h"

#include <assert.h>
#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( flame, FlameEffect )

void FlameEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( !windows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data, time);
    }

void FlameEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ))
        {
        if( windows[ w ] < 1 )
            {
            windows[ w ] += time / 500.;
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
            data.quads = data.quads.splitAtY( windows[ w ] * w->height());
            }
        else
            {
            windows.remove( w );
            w->unrefWindow();
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void FlameEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        WindowQuadList new_quads;
        double ylimit = windows[ w ] * w->height(); // parts above this are already away
        foreach( const WindowQuad &quad, data.quads )
            {
            if( quad.bottom() <= ylimit )
                continue;
            new_quads.append( quad );
            }
        if( new_quads.isEmpty())
            return; // nothing to paint
        data.quads = new_quads;
        }
    effects->paintWindow( w, mask, region, data );
    }

void FlameEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        effects->addRepaint( w->geometry()); // workspace, since the window under it will need painting too
    effects->postPaintScreen();
    }

void FlameEffect::windowClosed( EffectWindow* c )
    {
    windows[ c ] = 0;
    c->refWindow();
    }

void FlameEffect::windowDeleted( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
