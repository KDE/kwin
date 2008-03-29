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

#include "presentwindows.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>
#include <kdebug.h>

#include <QMouseEvent>
#include <QPainter>

#include <math.h>
#include <assert.h>
#include <limits.h>

namespace KWin
{

KWIN_EFFECT( presentwindows, PresentWindowsEffect )

PresentWindowsEffect::PresentWindowsEffect()
    : mShowWindowsFromAllDesktops ( false )
    , mActivated( false )
    , mActiveness( 0.0 )
    , mRearranging( 1.0 )
    , hasKeyboardGrab( false )
    , mHighlightedWindow( NULL )
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    , filterTexture( NULL )
#endif
    {
    KConfigGroup conf = effects->effectConfig("PresentWindows");

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Expose Effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F9));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
    KAction* b = (KAction*)actionCollection->addAction( "ExposeAll" );
    b->setText( i18n("Toggle Expose Effect (incl other desktops)" ));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));

    borderActivate = (ElectricBorder)conf.readEntry("BorderActivate", (int)ElectricNone);
    borderActivateAll = (ElectricBorder)conf.readEntry("BorderActivateAll", (int)ElectricTopLeft);
    drawWindowCaptions = conf.readEntry("DrawWindowCaptions", true);

    effects->reserveElectricBorder( borderActivate );
    effects->reserveElectricBorder( borderActivateAll );

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    alphaFormat = XRenderFindStandardFormat( display(), PictStandardARGB32 );
#endif
    }

PresentWindowsEffect::~PresentWindowsEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    effects->unreserveElectricBorder( borderActivateAll );
    discardFilterTexture();
    }


void PresentWindowsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    // How long does it take for the effect to get it's full strength (in ms)
    const double changeTime = 125;
    if(mActivated)
        {
        mActiveness = qMin(1.0, mActiveness + time/changeTime);
        if( mRearranging < 1 )
            mRearranging = qMin(1.0, mRearranging + time/changeTime);
        }
    else if(mActiveness > 0.0)
        {
        mActiveness = qMax(0.0, mActiveness - time/changeTime);
        if(mActiveness <= 0.0)
            effectTerminated();
        }

    // We need to mark the screen windows as transformed. Otherwise the whole
    //  screen won't be repainted, resulting in artefacts
    if( mActiveness > 0.0f )
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void PresentWindowsEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mActiveness > 0.0f )
        {
        if( mWindowData.contains(w) )
            {
            // This window will be transformed by the effect
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            // If it's minimized window or on another desktop and effect is not
            //  fully active, then apply some transparency
            if( mActiveness < 1.0f && (w->isMinimized() || !w->isOnCurrentDesktop() ))
                data.setTranslucent();
            // Change window's highlight
            WindowData& windata = mWindowData[w];
            const double highlightchangetime = 100;
            if( w == mHighlightedWindow )
                windata.highlight = qMin(1.0, windata.highlight + time / highlightchangetime);
            else
                windata.highlight = qMax(0.0, windata.highlight - time / highlightchangetime);
            }
        else if( !w->isDesktop())
            w->disablePainting( EffectWindow::PAINT_DISABLED );
        }
    effects->prePaintWindow( w, data, time );
    }

void PresentWindowsEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( filterTexture && region.intersects( filterFrameRect ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        // First render the frame
        QColor color = QPalette().color( QPalette::Active, QPalette::Highlight );
        glColor4f( color.redF(), color.greenF(), color.blueF(), 0.75f );
        renderRoundBoxWithEdge( filterFrameRect );
        // And then the text on top of it
        filterTexture->bind();
        filterTexture->render( region, filterTextureRect );
        filterTexture->unbind();
        glPopAttrib();
        }
#endif
    }

void PresentWindowsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if(mActiveness > 0.0f && mWindowData.contains(w))
        {
        // Change window's position and scale
        const WindowData& windata = mWindowData[w];
        if( mRearranging < 1 ) // rearranging
            {
            if( windata.old_area.isEmpty()) // no old position
                {
                data.xScale = windata.scale;
                data.yScale = windata.scale;
                data.xTranslate = windata.area.left() - w->x();
                data.yTranslate = windata.area.top() - w->y();
                data.opacity *= interpolate(0.0, 1.0, mRearranging);
                }
            else
                {
                data.xScale = interpolate(windata.old_scale, windata.scale, mRearranging);
                data.yScale = interpolate(windata.old_scale, windata.scale, mRearranging);
                data.xTranslate = (int)interpolate(windata.old_area.left() - w->x(),
                    windata.area.left() - w->x(), mRearranging);
                data.yTranslate = (int)interpolate(windata.old_area.top() - w->y(),
                    windata.area.top() - w->y(), mRearranging);
                }
            }
        else
            {
            data.xScale = interpolate(data.xScale, windata.scale, mActiveness);
            data.yScale = interpolate(data.xScale, windata.scale, mActiveness);
            data.xTranslate = (int)interpolate(data.xTranslate, windata.area.left() - w->x(), mActiveness);
            data.yTranslate = (int)interpolate(data.yTranslate, windata.area.top() - w->y(), mActiveness);
            }
        // Darken all windows except for the one under the cursor
        data.brightness *= interpolate(1.0, 0.7, mActiveness * (1.0f - windata.highlight));
        // If it's minimized window or on another desktop and effect is not
        //  fully active, then apply some transparency
        if( mActiveness < 1.0f && (w->isMinimized() || !w->isOnCurrentDesktop() ))
            data.opacity *= interpolate(0.0, 1.0, mActiveness);
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );

    if(mActiveness > 0.0f && mWindowData.contains(w))
        {
        const WindowData& windata = mWindowData[w];
        paintWindowIcon( w, data );

        if (drawWindowCaptions)
            {
            QString text = w->caption();
            double centerx = w->x() + data.xTranslate + w->width() * data.xScale * 0.5f;
            double centery = w->y() + data.yTranslate + w->height() * data.yScale * 0.5f;
            int maxwidth = (int)(w->width() * data.xScale - 20);
            double opacity = (0.7 + 0.2*windata.highlight) * data.opacity * mActiveness;
            QColor textcolor( 255, 255, 255, (int)(255*opacity) );
            QColor bgcolor( 0, 0, 0, (int)(255*opacity) );
            QFont f;
            f.setBold( true );
            f.setPointSize( 12 );
            effects->paintTextWithBackground( text, QPoint( (int)centerx, (int)centery ), maxwidth,
                                            textcolor, bgcolor, f);
            }
        }
    }

void PresentWindowsEffect::postPaintScreen()
    {
    if( mActivated && mActiveness < 1.0 ) // activating effect
        effects->addRepaintFull();
    if( mActivated && mRearranging < 1.0 ) // rearranging
        effects->addRepaintFull();
    if( !mActivated && mActiveness > 0.0 ) // deactivating effect
        effects->addRepaintFull();
    foreach( const WindowData& d, mWindowData )
        {
        if( d.highlight > 0 && d.highlight < 1 ) // changing highlight
            effects->addRepaintFull();
        }
    // Call the next effect.
    effects->postPaintScreen();
    }

void PresentWindowsEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == mInput );
    if( e->type() == QEvent::MouseMove )
        { // Repaint if the highlighted window changed.
          // (No need to use cursorMoved(), this takes care of it as well)
        for( DataHash::ConstIterator it = mWindowData.begin();
             it != mWindowData.end();
             ++it )
            {
            if( (*it).area.contains( cursorPos()))
                {
                if( mHighlightedWindow != it.key())
                    setHighlightedWindow( it.key());
                return;
                }
            }
        return;
        }
    if( e->type() != QEvent::MouseButtonPress )
        return;
    if( static_cast< QMouseEvent* >( e )->button() != Qt::LeftButton )
        {
        setActive( false );
        return;
        }

    // Find out which window (if any) was clicked and activate it
    QPoint pos = static_cast< QMouseEvent* >( e )->pos();
    for( DataHash::iterator it = mWindowData.begin();
         it != mWindowData.end(); ++it )
        {
        if( it.value().area.contains(pos) )
            {
            effects->activateWindow( it.key() );
            // mWindowData gets cleared and rebuilt when a window is
            // activated, so it's dangerous (and unnecessary) to continue
            break;
            }
        }

    // Deactivate effect, no matter if any window was actually activated
    setActive(false);
    }

void PresentWindowsEffect::windowClosed( EffectWindow* w )
    {
    if( mHighlightedWindow == w )
        setHighlightedWindow( findFirstWindow());
    mWindowsToPresent.removeAll( w );
    rearrangeWindows();
    }

void PresentWindowsEffect::setActive(bool active)
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( mActivated == active )
        return;
    mActivated = active;
    if( mActivated )
        {
        mWindowData.clear();
        effectActivated();
        mActiveness = 0;
        windowFilter.clear();
        mWindowsToPresent.clear();
        const EffectWindowList& originalwindowlist = effects->stackingOrder();
        // Filter out special windows such as panels and taskbars
        foreach( EffectWindow* window, originalwindowlist )
            {
            if( window->isSpecialWindow() )
                continue;
            if( window->isDeleted())
                continue;
            if( !mShowWindowsFromAllDesktops && !window->isOnCurrentDesktop() )
                continue;
            mWindowsToPresent.append(window);
            }
        rearrangeWindows();
        setHighlightedWindow( effects->activeWindow());
        }
    else
        {
        mWindowsToPresent.clear();
        mRearranging = 1; // turn off
        mActiveness = 1; // go back from arranged position
        discardFilterTexture();
        mHighlightedWindow = NULL;
        windowFilter.clear();
        }
    effects->addRepaintFull(); // trigger next animation repaint
    }

void PresentWindowsEffect::effectActivated()
    {
    // Create temporary input window to catch mouse events
    mInput = effects->createFullScreenInputWindow( this, Qt::PointingHandCursor );
    hasKeyboardGrab = effects->grabKeyboard( this );
    effects->setActiveFullScreenEffect( this );
    setHighlightedWindow( effects->activeWindow());
    }

void PresentWindowsEffect::effectTerminated()
    {
    mWindowData.clear();
    // Destroy the temporary input window
    effects->destroyInputWindow( mInput );
    if( hasKeyboardGrab )
        effects->ungrabKeyboard();
    hasKeyboardGrab = false;
    effects->setActiveFullScreenEffect( 0 );
    effects->addRepaintFull(); // to get rid of highlight
    }

void PresentWindowsEffect::rearrangeWindows()
    {
    if( !mActivated )
        return;

    EffectWindowList windowlist;
    if( windowFilter.isEmpty())
        windowlist = mWindowsToPresent;
    else
        {
        foreach( EffectWindow* w, mWindowsToPresent )
            {
            if( w->caption().contains( windowFilter, Qt::CaseInsensitive )
                || w->windowClass().contains( windowFilter, Qt::CaseInsensitive )
                || w->windowRole().contains( windowFilter, Qt::CaseInsensitive ))
                windowlist.append( w );
            }
        }
    if( windowlist.isEmpty())
        {
        mWindowData.clear();
        setHighlightedWindow( NULL );
        effects->addRepaintFull();
        return;
        }

    if( !mWindowData.isEmpty()) // this is not the first arranging
        {
        bool rearrange = canRearrangeClosest( windowlist ); // called before manipulating mWindowData
        DataHash newdata;
        EffectWindowList newlist = windowlist;
        EffectWindowList oldlist = mWindowData.keys();
        qSort( newlist );
        qSort( oldlist );
        for( DataHash::ConstIterator it = mWindowData.begin();
             it != mWindowData.end();
             ++it )
            if( windowlist.contains( it.key())) // remove windows that are not in the window list
                newdata[ it.key() ] = *it;
        mWindowData = newdata;
        if( !rearrange && newlist == oldlist )
            return;
        if( mHighlightedWindow != NULL && !mWindowData.contains( mHighlightedWindow ))
            setHighlightedWindow( NULL );
        for( DataHash::Iterator it = mWindowData.begin();
             it != mWindowData.end();
             ++it )
            {
            (*it).old_area = (*it).area;
            (*it).old_scale = (*it).scale;
            }
        // Initialize new entries
        foreach( EffectWindow* w, windowlist )
            if( !mWindowData.contains( w ))
                {
                mWindowData[ w ].highlight = 0;
                }
        mRearranging = 0; // start animation again
        }

    // Calculate new positions and scales for windows
//    calculateWindowTransformationsDumb( windowlist );
//    calculateWindowTransformationsKompose( windowlist );
    calculateWindowTransformationsClosest( windowlist );

    if( !mWindowData.isEmpty() && mHighlightedWindow == NULL )
        setHighlightedWindow( findFirstWindow());

    // Schedule entire desktop to be repainted
    effects->addRepaintFull();
    }

void PresentWindowsEffect::calculateWindowTransformationsDumb(EffectWindowList windowlist)
    {
    // Calculate number of rows/cols
    int rows = windowlist.count() / 4 + 1;
    int cols = windowlist.count() / rows + windowlist.count() % rows;
    // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect placementRect = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    // Size of one cell
    int cellwidth = placementRect.width() / cols;
    int cellheight = placementRect.height() / rows;
    kDebug() << "Got " << windowlist.count() << " clients, using " << rows << "x" << cols << " grid";

    // Calculate position and scale factor for each window
    int i = 0;
    foreach( EffectWindow* window, windowlist )
        {

        // Row/Col of this window
        int r = i / cols;
        int c = i % cols;
        mWindowData[window].slot = i;
        mWindowData[window].x = c;
        mWindowData[window].y = r;
        mWindowData[window].highlight = 0.0f;
        mWindowData[window].scale = qMin(cellwidth / (double)window->width(), cellheight / (double)window->height());
        mWindowData[window].area.setLeft(placementRect.left() + cellwidth * c);
        mWindowData[window].area.setTop(placementRect.top() + cellheight * r);
        mWindowData[window].area.setWidth((int)(window->width() * mWindowData[window].scale));
        mWindowData[window].area.setHeight((int)(window->height() * mWindowData[window].scale));

        kDebug() << "Window '" << window->caption() << "' gets moved to (" <<
            mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            "), scale: " << mWindowData[window].scale << endl;
        i++;
        }
    }

double PresentWindowsEffect::windowAspectRatio(EffectWindow* c)
    {
    return c->width() / (double)c->height();
    }

int PresentWindowsEffect::windowWidthForHeight(EffectWindow* c, int h)
    {
    return (int)((h / (double)c->height()) * c->width());
    }

int PresentWindowsEffect::windowHeightForWidth(EffectWindow* c, int w)
    {
    return (int)((w / (double)c->width()) * c->height());
    }

void PresentWindowsEffect::calculateWindowTransformationsKompose(EffectWindowList windowlist)
    {
     // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect availRect = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp

    int spacing = 10;
    int rows, columns;
    double parentRatio = availRect.width() / (double)availRect.height();
    // Use more columns than rows when parent's width > parent's height
    if ( parentRatio > 1 )
    {
        columns = (int)ceil( sqrt((double)windowlist.count()) );
        rows = (int)ceil( (double)windowlist.count() / (double)columns );
    }
    else
    {
        rows = (int)ceil( sqrt((double)windowlist.count()) );
        columns = (int)ceil( (double)windowlist.count() / (double)rows );
    }
    kDebug() << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients";

    // Calculate width & height
    int w = (availRect.width() - (columns+1) * spacing ) / columns;
    int h = (availRect.height() - (rows+1) * spacing ) / rows;

    EffectWindowList::iterator it( windowlist.begin() );
    QList<QRect> geometryRects;
    QList<int> maxRowHeights;
    // Process rows
    for ( int i=0; i<rows; ++i )
        {
        int xOffsetFromLastCol = 0;
        int maxHeightInRow = 0;
        // Process columns
        for ( int j=0; j<columns; ++j )
            {
            EffectWindow* window;

            // Check for end of List
            if ( it == windowlist.end() )
                break;
            window = *it;

            // Calculate width and height of widget
            double ratio = windowAspectRatio(window);

            int widgetw = 100;
            int widgeth = 100;
            int usableW = w;
            int usableH = h;

            // use width of two boxes if there is no right neighbour
            if (window == windowlist.last() && j != columns-1)
                {
                usableW = 2*w;
                }
            ++it; // We need access to the neighbour in the following
            // expand if right neighbour has ratio < 1
            if (j != columns-1 && it != windowlist.end() && windowAspectRatio(*it) < 1)
                {
                int addW = w - windowWidthForHeight(*it, h);
                if ( addW > 0 )
                    {
                    usableW = w + addW;
                    }
                }

            if ( ratio == -1 )
                {
                widgetw = w;
                widgeth = h;
                }
            else
                {
                double widthForHeight = windowWidthForHeight(window, usableH);
                double heightForWidth = windowHeightForWidth(window, usableW);
                if ( (ratio >= 1.0 && heightForWidth <= usableH) ||
                      (ratio < 1.0 && widthForHeight > usableW)   )
                    {
                    widgetw = usableW;
                    widgeth = (int)heightForWidth;
                    }
                else if ( (ratio < 1.0 && widthForHeight <= usableW) ||
                           (ratio >= 1.0 && heightForWidth > usableH)   )
                    {
                    widgeth = usableH;
                    widgetw = (int)widthForHeight;
                    }
                }

            // Set the Widget's size

            int alignmentXoffset = 0;
            int alignmentYoffset = 0;
            if ( i==0 && h > widgeth )
                alignmentYoffset = h - widgeth;
            if ( j==0 && w > widgetw )
                alignmentXoffset = w - widgetw;
            QRect geom( availRect.x() + j * (w + spacing) + spacing + alignmentXoffset + xOffsetFromLastCol,
                        availRect.y() + i * (h + spacing) + spacing + alignmentYoffset,
                        widgetw, widgeth );
            geometryRects.append(geom);

      // Set the x offset for the next column
            if (alignmentXoffset==0)
                xOffsetFromLastCol += widgetw-w;
            if (maxHeightInRow < widgeth)
                maxHeightInRow = widgeth;
        }
        maxRowHeights.append(maxHeightInRow);
    }

    int topOffset = 0;
    for( int i = 0; i < rows; i++ )
        {
        for( int j = 0; j < columns; j++ )
            {
            int pos = i*columns + j;
            if(pos >= windowlist.count())
                break;

            EffectWindow* window = windowlist[pos];
            QRect geom = geometryRects[pos];
            geom.setY( geom.y() + topOffset );
            mWindowData[window].slot = pos;
            mWindowData[window].x = j;
            mWindowData[window].y = i;
            mWindowData[window].area = geom;
            mWindowData[window].scale = geom.width() / (double)window->width();
            mWindowData[window].highlight = 0.0f;

            kDebug() << "Window '" << window->caption() << "' gets moved to (" <<
                    mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
                    "), scale: " << mWindowData[window].scale << endl;
            }
        if ( maxRowHeights[i]-h > 0 )
            topOffset += maxRowHeights[i]-h;
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist)
    {
    QRect area = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    int columns = int( ceil( sqrt( (double)windowlist.count())));
    int rows = int( ceil( windowlist.count() / double( columns )));
    foreach( EffectWindow* w, windowlist )
        mWindowData[ w ].slot = -1;
    for(;;)
        {
        // Assign each window to the closest available slot
        assignSlots( area, columns, rows );
        // Leave only the closest window in each slot, remove further conflicts
        getBestAssignments();
        bool all_assigned = true;
        foreach( EffectWindow* w, windowlist )
            if( mWindowData[ w ].slot == -1 )
                {
                all_assigned = false;
                break;
                }
        if( all_assigned )
            break; // ok
        }
    int slotwidth = area.width() / columns;
    int slotheight = area.height() / rows;
    for( DataHash::Iterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        {
        QRect geom( area.x() + ((*it).slot % columns ) * slotwidth,
            area.y() + ((*it).slot / columns ) * slotheight,
            slotwidth, slotheight );
        geom.adjust( 10, 10, -10, -10 ); // borders
        double scale;
        EffectWindow* w = it.key();
        if( geom.width() / double( w->width()) < geom.height() / double( w->height()))
            { // center vertically
            scale = geom.width() / double( w->width());
            geom.moveTop( geom.top() + ( geom.height() - int( w->height() * scale )) / 2 );
            geom.setHeight( int( w->height() * scale ));
            }
        else
            { // center horizontally
            scale = geom.height() / double( w->height());
            geom.moveLeft( geom.left() + ( geom.width() - int( w->width() * scale )) / 2 );
            geom.setWidth( int( w->width() * scale ));
            }
        // Don't scale the windows too much
        if( scale > 2.0 )
        {
            scale = 2.0;
            QPoint center = geom.center();
            geom = QRect( geom.center().x() - w->width(), geom.center().y() - w->height(),
                          2 * w->width(), 2 * w->height() );
        }
        (*it).area = geom;
        (*it).scale = scale;
        }
    }

void PresentWindowsEffect::assignSlots( const QRect& area, int columns, int rows )
    {
    QVector< bool > taken;
    taken.fill( false, columns * rows );
    foreach( const WindowData& d, mWindowData )
        {
        if( d.slot != -1 )
            taken[ d.slot ] = true;
        }
    int slotwidth = area.width() / columns;
    int slotheight = area.height() / rows;
    for( DataHash::Iterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        {
        if( (*it).slot != -1 )
            continue; // it already has a slot
        QPoint pos = it.key()->geometry().center();
        if( pos.x() < area.left())
            pos.setX( area.left());
        if( pos.x() > area.right())
            pos.setX( area.right());
        if( pos.y() < area.top())
            pos.setY( area.top());
        if( pos.y() > area.bottom())
            pos.setY( area.bottom());
        int distance = INT_MAX;
        for( int x = 0;
             x < columns;
             ++x )
            for( int y = 0;
                 y < rows;
                 ++y )
                {
                int slot = x + y * columns;
                if( taken[ slot ] )
                    continue;
                int xdiff = pos.x() - ( area.x() + slotwidth * x + slotwidth / 2 ); // slotwidth/2 for center
                int ydiff = pos.y() - ( area.y() + slotheight * y + slotheight / 2 );
                int dist = int( sqrt( (double)(xdiff * xdiff + ydiff * ydiff) ));
                if( dist < distance )
                    {
                    distance = dist;
                    (*it).slot = slot;
                    (*it).x = x;
                    (*it).y = y;
                    (*it).slot_distance = distance;
                    }
                }
        }
    }

void PresentWindowsEffect::getBestAssignments()
    {
    for( DataHash::Iterator it1 = mWindowData.begin();
         it1 != mWindowData.end();
         ++it1 )
        {
        for( DataHash::ConstIterator it2 = mWindowData.begin();
             it2 != mWindowData.end();
             ++it2 )
            {
            if( it1.key() != it2.key() && (*it1).slot == (*it2).slot
                && (*it1).slot_distance >= (*it2).slot_distance )
                {
                (*it1).slot = -1;
                }
            }
        }
    }

bool PresentWindowsEffect::canRearrangeClosest(EffectWindowList windowlist)
    {
    QRect area = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    int columns = int( ceil( sqrt( (double)windowlist.count())));
    int rows = int( ceil( windowlist.count() / double( columns )));
    int old_columns = int( ceil( sqrt( (double)mWindowData.count())));
    int old_rows = int( ceil( mWindowData.count() / double( columns )));
    return old_columns != columns || old_rows != rows;
    }

bool PresentWindowsEffect::borderActivated( ElectricBorder border )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return false;
    if( border == borderActivate && !mActivated )
        {
        toggleActive();
        return true;
        }
    if( border == borderActivateAll && !mActivated )
        {
        toggleActiveAllDesktops();
        return true;
        }
    return false;
    }

void PresentWindowsEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    if( e->type() == QEvent::KeyPress )
        {
        switch( e->key())
            { // wrap only on autorepeat
            case Qt::Key_Left:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, -1, 0, !e->isAutoRepeat()));
                break;
            case Qt::Key_Right:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 1, 0, !e->isAutoRepeat()));
                break;
            case Qt::Key_Up:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 0, -1, !e->isAutoRepeat()));
                break;
            case Qt::Key_Down:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 0, 1, !e->isAutoRepeat()));
                break;
            case Qt::Key_Home:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, -1000, 0, false ));
                break;
            case Qt::Key_End:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 1000, 0, false ));
                break;
            case Qt::Key_PageUp:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 0, -1000, false ));
                break;
            case Qt::Key_PageDown:
                setHighlightedWindow( relativeWindow( mHighlightedWindow, 0, 1000, false ));
                break;
            case Qt::Key_Backspace:
                if( !windowFilter.isEmpty())
                    {
                    windowFilter.remove( windowFilter.length() - 1, 1 );
                    updateFilterTexture();
                    rearrangeWindows();
                    }
                return;
            case Qt::Key_Escape:
                setActive( false );
                return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if( mHighlightedWindow != NULL )
                    {
                    effects->activateWindow( mHighlightedWindow );
                    setActive( false );
                    return;
                    }
                if( mWindowData.count() == 1 ) // only one window shown
                    {
                    effects->activateWindow( mWindowData.begin().key());
                    setActive( false );
                    }
                return;
            default:
                if( !e->text().isEmpty())
                    {
                    windowFilter.append( e->text());
                    updateFilterTexture();
                    rearrangeWindows();
                    return;
                    }
                break;
            }
        }
    }

void PresentWindowsEffect::setHighlightedWindow( EffectWindow* w )
    {
    if( w == mHighlightedWindow || ( w != NULL && !mWindowData.contains( w )))
        return;
    effects->addRepaintFull(); // everything is transformed anyway
    mHighlightedWindow = w;
    }

// returns a window which is to relative position <xdiff,ydiff> from the given window
EffectWindow* PresentWindowsEffect::relativeWindow( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const
    {
    if( mWindowData.count() == 0 )
        return NULL;
    if( w == NULL )
        return findFirstWindow();
    int columns = int( ceil( sqrt( (double)mWindowData.count())));
    int rows = int( ceil( mWindowData.count() / double( columns )));
    QVector< QVector< EffectWindow* > > grid;
    grid.resize( columns );
    for( int i = 0;
         i < columns;
         ++i )
        grid[ i ].resize( rows );
    for( DataHash::ConstIterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        grid[ it->x ][ it->y ] = it.key();
    int x = mWindowData[ w ].x;
    int y = mWindowData[ w ].y;
    while( xdiff > 0 )
        {
        ++x;
        if( x == columns )
            {
            if( !wrap )
                { // make sure to find the leftmost (or 'w', which is guaranteed)
                --x;
                while( x >= 0 && grid[ x ][ y ] == NULL )
                    --x;
                break;
                }
            else
                x = 0;
            }
        if( grid[ x ][ y ] != NULL )
            --xdiff;
        }
    while( xdiff < 0 )
        {
        --x;
        if( x < 0 )
            {
            if( !wrap )
                {
                ++x;
                while( x <= columns - 1 && grid[ x ][ y ] == NULL )
                    ++x;
                break;
                }
            else
                x = columns - 1;
            }
        if( grid[ x ][ y ] != NULL )
            ++xdiff;
        }
    while( ydiff > 0 )
        {
        ++y;
        if( y == rows )
            {
            if( !wrap )
                {
                --y;
                while( y >= 0 && grid[ x ][ y ] == NULL )
                    --y;
                break;
                }
            else
                y = 0;
            }
        if( grid[ x ][ y ] != NULL )
            --ydiff;
        }
    while( ydiff < 0 )
        {
        --y;
        if( y < 0 )
            {
            if( !wrap )
                {
                ++y;
                while( y <= rows - 1 && grid[ x ][ y ] == NULL )
                    ++y;
                break;
                }
            else
                y = rows - 1;
            }
        if( grid[ x ][ y ] != NULL )
            ++ydiff;
        }
    return grid[ x ][ y ];
    }

// returns the window that is the most to the topleft, if any
EffectWindow* PresentWindowsEffect::findFirstWindow() const
    {
    int minslot = INT_MAX;
    EffectWindow* ret = NULL;
    for( DataHash::ConstIterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        {
        if( (*it).slot < minslot )
            {
            minslot = (*it).slot;
            ret = it.key();
            }
        }
    return ret;
    }

void PresentWindowsEffect::discardFilterTexture()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    delete filterTexture;
    filterTexture = NULL;
#endif
    }

void PresentWindowsEffect::updateFilterTexture()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    discardFilterTexture();
    if( windowFilter.isEmpty())
        {
        effects->addRepaint( filterTextureRect );
        return;
        }
    // Create font for filter text
    QFont font;
    font.setPointSize( font.pointSize() * 2 );
    font.setBold( true );
    // Get size of the rect containing filter text
    QFontMetrics fm( font );
    QRect rect;
    QString translatedString = i18n( "Filter:\n%1", windowFilter );
    rect.setSize( fm.size( 0, translatedString ));
    QRect area = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    // Create image
    QImage im( rect.width(), rect.height(), QImage::Format_ARGB32 );
    im.fill( Qt::transparent );
    // Paint the filter text to it
    QPainter p( &im );
    p.setFont( font );
    p.setPen( QPalette().color( QPalette::Active, QPalette::HighlightedText ) );
    p.drawText( rect, Qt::AlignCenter, translatedString );
    p.end();
    // Create GL texture
    filterTexture = new GLTexture( im );
    // Get position for filter text and it's frame
    filterTextureRect = QRect( area.x() + ( area.width() - rect.width()) / 2,
        area.y() + ( area.height() - rect.height()) / 2, rect.width(), rect.height());
    const int borderh = 10;
    const int borderw = 20;
    filterFrameRect = filterTextureRect.adjusted( -borderw, -borderh, borderw, borderh );
    // Schedule repaint
    effects->addRepaint( filterTextureRect );
#endif
    }

void PresentWindowsEffect::paintWindowIcon( EffectWindow* w, WindowPaintData& paintdata )
    {
    // Don't render null icons
    if( w->icon().isNull() )
        {
        return;
        }

    WindowData& data = mWindowData[ w ];
    // TODO: find out why this doesn't work properly
    // if( data.icon.cacheKey() != w->icon().cacheKey())
        { // make sure data.icon is the right QPixmap, and rebind
        data.icon = w->icon();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing )
            {
            data.iconTexture.load( data.icon );
            data.iconTexture.setFilter( GL_LINEAR );
            }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing )
            {
            if( data.iconPicture != None )
                XRenderFreePicture( display(), data.iconPicture );
            data.iconPicture = XRenderCreatePicture( display(),
                data.icon.handle(), alphaFormat, 0, NULL );
            }
#endif
        }
    int icon_margin = 8;
    int width = data.icon.width();
    int height = data.icon.height();
    int x = w->x() + paintdata.xTranslate + w->width() * paintdata.xScale * 0.95 - width - icon_margin;
    int y = w->y() + paintdata.yTranslate + w->height() * paintdata.yScale * 0.95 - height - icon_margin;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        // Render some background
        glColor4f( 0, 0, 0, 0.5 * mActiveness );
        renderRoundBox( QRect( x-3, y-3, width+6, height+6 ), 3 );
        // Render the icon
        glColor4f( 1, 1, 1, 1 * mActiveness );
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        data.iconTexture.bind();
        const float verts[ 4 * 2 ] =
            {
            x, y,
            x, y + height,
            x + width, y + height,
            x + width, y
            };
        const float texcoords[ 4 * 2 ] =
            {
            0, 1,
            0, 0,
            1, 0,
            1, 1
            };
        renderGLGeometry( 4, verts, texcoords );
        data.iconTexture.unbind();
        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        XRenderComposite( display(),
            data.icon.depth() == 32 ? PictOpOver : PictOpSrc,
            data.iconPicture, None,
            effects->xrenderBufferPicture(),
            0, 0, 0, 0, x, y, width, height );
        }
#endif
    }

} // namespace
#include "presentwindows.moc"
