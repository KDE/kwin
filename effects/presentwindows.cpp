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



namespace KWin
{

KWIN_EFFECT( PresentWindows, PresentWindowsEffect )

PresentWindowsEffect::PresentWindowsEffect() : QObject(), Effect()
    {
    mShowWindowsFromAllDesktops = false;
    mActivated = false;
    mActiveness = 0.0;

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Expose effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
    KAction* b = (KAction*)actionCollection->addAction( "ExposeAll" );
    b->setText( i18n("Toggle Expose effect (incl other desktops)" ));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));

    borderActivate = ElectricTopRight;
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
    if( mActiveness > 0.0f && mWindowData.contains(w) )
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
    // If mActiveness is between 0 and 1, the effect is still in progress and the
    //  workspace has to be repainted during the next pass
    if( mActiveness > 0.0 && mActiveness < 1.0 )
        effects->addRepaintFull(); // trigger next animation repaint

    // Call the next effect.
    effects->postPaintScreen();
    }

void PresentWindowsEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == mInput );
    if( e->type() != QEvent::MouseButtonPress )
        return;

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

void PresentWindowsEffect::windowActivated( EffectWindow* )
    {
        rearrangeWindows();
    }

void PresentWindowsEffect::windowClosed( EffectWindow* )
    {
        rearrangeWindows();
    }

void PresentWindowsEffect::setActive(bool active)
    {
    mActivated = active;
    rearrangeWindows();
    if( mActivated && mActiveness == 0.0f )
        effectActivated();
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

    const EffectWindowList& originalwindowlist = effects->stackingOrder();
    // Filter out special windows such as panels and taskbars
    EffectWindowList windowlist;
    foreach( EffectWindow* window, originalwindowlist )
        {
        if( window->isSpecialWindow() )
            continue;
        if( !mShowWindowsFromAllDesktops && !window->isOnCurrentDesktop() )
            continue;
        windowlist.append(window);
        }

    // Calculate new positions and scales for windows
    calculateWindowTransformationsKompose( windowlist );

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
    QRect availRect = effects->clientArea( PlacementArea, QPoint( 0, 0 ), 0 );

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
