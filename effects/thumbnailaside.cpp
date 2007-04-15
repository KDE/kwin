/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "thumbnailaside.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>

namespace KWin
{

KWIN_EFFECT( ThumbnailAside, ThumbnailAsideEffect )

ThumbnailAsideEffect::ThumbnailAsideEffect()
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "ToggleCurrentThumbnail" );
    a->setText( i18n("Toggle Thumbnail for Current Window" ));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F9));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleCurrentThumbnail()));
    maxwidth = 200;
    spacing = 10; // TODO config options?
    opacity = 0.5;
    }

void ThumbnailAsideEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    foreach( const Data& d, windows )
        {
        if( region.contains( d.rect ))
            {
            WindowPaintData data;
            data.opacity = opacity;
            QRect region;
            setPositionTransformations( data, region, d.window, d.rect, Qt::KeepAspectRatio );
            effects->drawWindow( d.window, PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED,
                region, data );
            }
        }
    }

void ThumbnailAsideEffect::windowDamaged( EffectWindow* w, const QRect& )
    {
    foreach( const Data& d, windows )
        {
        if( d.window == w )
            effects->addRepaint( d.rect );
        }
    }

void ThumbnailAsideEffect::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( w->size() == old.size())
        {
        foreach( const Data& d, windows )
            {
            if( d.window == w )
                effects->addRepaint( d.rect );
            }
        }
    else
        arrange();
    }

void ThumbnailAsideEffect::windowClosed( EffectWindow* w )
    {
    removeThumbnail( w );
    }

void ThumbnailAsideEffect::toggleCurrentThumbnail()
    {
    EffectWindow* active = effects->activeWindow();
    if( active == NULL )
        return;
    if( windows.contains( active ))
        removeThumbnail( active );
    else
        addThumbnail( active );
    }

void ThumbnailAsideEffect::addThumbnail( EffectWindow* w )
    {
    repaintAll(); // repaint old areas
    Data d;
    d.window = w;
    d.index = windows.count();
    windows[ w ] = d;
    arrange();
    }

void ThumbnailAsideEffect::removeThumbnail( EffectWindow* w )
    {
    if( !windows.contains( w ))
        return;
    repaintAll(); // repaint old areas
    int index = windows[ w ].index;
    windows.remove( w );
    for( QHash< EffectWindow*, Data >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        Data& d = *it;
        if( d.index > index )
            --d.index;
        }
    arrange();
    }

void ThumbnailAsideEffect::arrange()
    {
    int height = 0;
    QVector< int > pos( windows.size());
    int mwidth = 0;
    foreach( const Data& d, windows )
        {
        height += d.window->height();
        mwidth = qMax( mwidth, d.window->width());
        pos[ d.index ] = d.window->height();
        }
    QRect area = effects->clientArea( WorkArea, QPoint(), effects->currentDesktop());
    double scale = area.height() / double( height );
    scale = qMin( scale, maxwidth / double( mwidth )); // don't be wider than maxwidth pixels
    int add = 0;
    for( int i = 0;
         i < windows.size();
         ++i )
        {
        pos[ i ] = int( pos[ i ] * scale );
        pos[ i ] += spacing + add; // compute offset of each item
        add = pos[ i ];
        }
    for( QHash< EffectWindow*, Data >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        Data& d = *it;
        int width = int( d.window->width() * scale );
        d.rect = QRect( area.right() - width, area.bottom() - pos[ d.index ], width, int( d.window->height() * scale ));
        }
    repaintAll();        
    }

void ThumbnailAsideEffect::repaintAll()
    {
    foreach( const Data& d, windows )
        effects->addRepaint( d.rect );
    }

} // namespace

#include "thumbnailaside.moc"
