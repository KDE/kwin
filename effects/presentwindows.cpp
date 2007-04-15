/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "presentwindows.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>

#include <QMouseEvent>

#include <math.h>
#include <assert.h>
#include <limits.h>

namespace KWin
{

KWIN_EFFECT( PresentWindows, PresentWindowsEffect )

PresentWindowsEffect::PresentWindowsEffect()
    : mShowWindowsFromAllDesktops ( false )
    , mActivated( false )
    , mActiveness( 0.0 )
    , mHoverWindow( NULL )
    {

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Expose effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
    KAction* b = (KAction*)actionCollection->addAction( "ExposeAll" );
    b->setText( i18n("Toggle Expose effect (incl other desktops)" ));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));

    borderActivate = ElectricTopRight; // TODO config options
    borderActivateAll = ElectricNone;

    effects->reserveElectricBorder( borderActivate );
    effects->reserveElectricBorder( borderActivateAll );
    }

PresentWindowsEffect::~PresentWindowsEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    effects->unreserveElectricBorder( borderActivateAll );
    }


void PresentWindowsEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    // How long does it take for the effect to get it's full strength (in ms)
    const float changeTime = 300;
    if(mActivated)
        mActiveness = qMin(1.0f, mActiveness + time/changeTime);
    else if(mActiveness > 0.0f)
        {
        mActiveness = qMax(0.0f, mActiveness - time/changeTime);
        if(mActiveness <= 0.0f)
            effectTerminated();
        }

    // We need to mark the screen windows as transformed. Otherwise the whole
    //  screen won't be repainted, resulting in artefacts
    if( mActiveness > 0.0f )
        *mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(mask, region, time);
    }

void PresentWindowsEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( mActiveness > 0.0f )
        {
        if( mWindowData.contains(w) )
            {
            // This window will be transformed by the effect
            *mask |= Effect::PAINT_WINDOW_TRANSFORMED;
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            // If it's minimized window or on another desktop and effect is not
            //  fully active, then apply some transparency
            if( mActiveness < 1.0f && (w->isMinimized() || !w->isOnCurrentDesktop() ))
                *mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
            // Change window's hover according to cursor pos
            WindowData& windata = mWindowData[w];
            const float hoverchangetime = 200;
            if( windata.area.contains(cursorPos()) )
                windata.hover = qMin(1.0f, windata.hover + time / hoverchangetime);
            else
                windata.hover = qMax(0.0f, windata.hover - time / hoverchangetime);
            }
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED );
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void PresentWindowsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if(mActiveness > 0.0f && mWindowData.contains(w))
        {
        // Change window's position and scale
        const WindowData& windata = mWindowData[w];
        data.xScale = interpolate(data.xScale, windata.scale, mActiveness);
        data.yScale = interpolate(data.xScale, windata.scale, mActiveness);
        data.xTranslate = (int)interpolate(data.xTranslate, windata.area.left() - w->x(), mActiveness);
        data.yTranslate = (int)interpolate(data.yTranslate, windata.area.top() - w->y(), mActiveness);
        // Darken all windows except for the one under the cursor
        data.brightness *= interpolate(1.0, 0.7, mActiveness * (1.0f - windata.hover));
        // If it's minimized window or on another desktop and effect is not
        //  fully active, then apply some transparency
        if( mActiveness < 1.0f && (w->isMinimized() || !w->isOnCurrentDesktop() ))
            data.opacity *= interpolate(0.0, 1.0, mActiveness);
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void PresentWindowsEffect::postPaintScreen()
    {
    if( mActivated && mActiveness < 1.0 ) // activating effect
        effects->addRepaintFull();
    if( !mActivated && mActiveness > 0.0 ) // deactivating effect
        effects->addRepaintFull();
    foreach( const WindowData& d, mWindowData )
        {
        if( d.hover > 0 && d.hover < 1 ) // changing highlight
            effects->addRepaintFull();
        }
    // Call the next effect.
    effects->postPaintScreen();
    }

void PresentWindowsEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == mInput );
    if( e->type() == QEvent::MouseMove )
        { // Repaint if the hovered-over window changed.
          // (No need to use cursorMoved(), this takes care of it as well)
        for( QHash<EffectWindow*, WindowData>::ConstIterator it = mWindowData.begin();
             it != mWindowData.end();
             ++it )
            {
            if( (*it).area.contains( cursorPos()))
                {
                if( mHoverWindow != it.key())
                    {
                    mHoverWindow = it.key();
                    effects->addRepaintFull(); // screen is transformed, so paint all
                    }
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
    for( QHash<EffectWindow*, WindowData>::iterator it = mWindowData.begin();
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
    if( mHoverWindow == w )
        mHoverWindow = NULL;
    mWindowsToPresent.remove( w );
    rearrangeWindows();
    }

void PresentWindowsEffect::setActive(bool active)
    {
    if( mActivated == active )
        return;
    mActivated = active;
    mHoverWindow = NULL;
    if( mActivated )
        {
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
        }
    else
        mWindowsToPresent.clear();
    rearrangeWindows();
    if( mActivated && mActiveness == 0.0f )
        effectActivated();
    effects->addRepaintFull(); // trigger next animation repaint
    }

void PresentWindowsEffect::effectActivated()
    {
    // Create temporary input window to catch mouse events
    mInput = effects->createFullScreenInputWindow( this, Qt::PointingHandCursor );
    // TODO: maybe also create a KAction so that ressing Esc would terminate the effect?
    }

void PresentWindowsEffect::effectTerminated()
    {
    // Destroy the temporary input window
    effects->destroyInputWindow( mInput );
    }

void PresentWindowsEffect::rearrangeWindows()
    {
    if( !mActivated )
        return;

    mWindowData.clear();

    EffectWindowList windowlist = mWindowsToPresent;
    // Calculate new positions and scales for windows
//    calculateWindowTransformationsDumb( windowlist );
//    calculateWindowTransformationsKompose( windowlist );
    calculateWindowTransformationsClosest( windowlist );

    // Schedule entire desktop to be repainted
    effects->addRepaintFull();
    }

void PresentWindowsEffect::calculateWindowTransformationsDumb(EffectWindowList windowlist)
    {
    // Calculate number of rows/cols
    int rows = windowlist.count() / 4 + 1;
    int cols = windowlist.count() / rows + windowlist.count() % rows;
    // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect placementRect = effects->clientArea( PlacementArea, QPoint( 0, 0 ), 0 );
    // Size of one cell
    int cellwidth = placementRect.width() / cols;
    int cellheight = placementRect.height() / rows;
    kDebug() << k_funcinfo << "Got " << windowlist.count() << " clients, using " << rows << "x" << cols << " grid" << endl;

    // Calculate position and scale factor for each window
    int i = 0;
    foreach( EffectWindow* window, windowlist )
        {

        // Row/Col of this window
        int r = i / cols;
        int c = i % cols;
        mWindowData[window].hover = 0.0f;
        mWindowData[window].scale = qMin(cellwidth / (float)window->width(), cellheight / (float)window->height());
        mWindowData[window].area.setLeft(placementRect.left() + cellwidth * c);
        mWindowData[window].area.setTop(placementRect.top() + cellheight * r);
        mWindowData[window].area.setWidth((int)(window->width() * mWindowData[window].scale));
        mWindowData[window].area.setHeight((int)(window->height() * mWindowData[window].scale));

        kDebug() << k_funcinfo << "Window '" << window->caption() << "' gets moved to (" <<
            mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            "), scale: " << mWindowData[window].scale << endl;
        i++;
        }
    }

float PresentWindowsEffect::windowAspectRatio(EffectWindow* c)
    {
    return c->width() / (float)c->height();
    }

int PresentWindowsEffect::windowWidthForHeight(EffectWindow* c, int h)
    {
    return (int)((h / (float)c->height()) * c->width());
    }

int PresentWindowsEffect::windowHeightForWidth(EffectWindow* c, int w)
    {
    return (int)((w / (float)c->width()) * c->height());
    }

void PresentWindowsEffect::calculateWindowTransformationsKompose(EffectWindowList windowlist)
    {
     // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect availRect = effects->clientArea( PlacementArea, QPoint( 0, 0 ), effects->currentDesktop());

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp

    int spacing = 10;
    int rows, columns;
    float parentRatio = availRect.width() / (float)availRect.height();
    // Use more columns than rows when parent's width > parent's height
    if ( parentRatio > 1 )
    {
        columns = (int)ceil( sqrt(windowlist.count()) );
        rows = (int)ceil( (double)windowlist.count() / (double)columns );
    }
    else
    {
        rows = (int)ceil( sqrt(windowlist.count()) );
        columns = (int)ceil( (double)windowlist.count() / (double)rows );
    }
    kDebug() << k_funcinfo << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients" << endl;

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
            float ratio = windowAspectRatio(window);

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
            mWindowData[window].area = geom;
            mWindowData[window].scale = geom.width() / (float)window->width();
            mWindowData[window].hover = 0.0f;

            kDebug() << k_funcinfo << "Window '" << window->caption() << "' gets moved to (" <<
                    mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
                    "), scale: " << mWindowData[window].scale << endl;
            }
        if ( maxRowHeights[i]-h > 0 )
            topOffset += maxRowHeights[i]-h;
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist)
    {
    QRect area = effects->clientArea( PlacementArea, QPoint( 0, 0 ), effects->currentDesktop());
    int columns = int( ceil( sqrt( windowlist.count())));
    int rows = int( ceil( windowlist.count() / double( columns )));
    foreach( EffectWindow* w, windowlist )
        {
        WindowData d;
        d.slot = -1;
        d.hover = 0;  // other data will be computed later
        mWindowData[ w ] = d;
        }
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
    for( QHash<EffectWindow*, WindowData>::Iterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        {
        QRect geom( area.x() + ((*it).slot % columns ) * slotwidth,
            area.y() + ((*it).slot / columns ) * slotheight,
            slotwidth, slotheight );
        geom.adjust( 10, 10, -10, -10 ); // borders
        (*it).area = geom;
        (*it).scale = geom.width() / float( it.key()->width());
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
    for( QHash<EffectWindow*, WindowData>::Iterator it = mWindowData.begin();
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
                int dist = int( sqrt( xdiff * xdiff + ydiff * ydiff ));
                if( dist < distance )
                    {
                    distance = dist;
                    (*it).slot = slot;
                    (*it).slot_distance = distance;
                    }
                }
        }
    }

void PresentWindowsEffect::getBestAssignments()
    {
    for( QHash<EffectWindow*, WindowData>::Iterator it1 = mWindowData.begin();
         it1 != mWindowData.end();
         ++it1 )
        {
        for( QHash<EffectWindow*, WindowData>::ConstIterator it2 = mWindowData.begin();
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

bool PresentWindowsEffect::borderActivated( ElectricBorder border )
    {
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

} // namespace
#include "presentwindows.moc"
