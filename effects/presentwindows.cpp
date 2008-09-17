/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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
    , mActiveness()
    , mRearranging()
    , hasKeyboardGrab( false )
    , mHighlightedWindow( NULL )
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    , filterTexture( NULL )
#endif
    , mTabBoxMode( false )
    {
    KConfigGroup conf = effects->effectConfig("PresentWindows");

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Present Windows (Current desktop)" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F9));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
    KAction* b = (KAction*)actionCollection->addAction( "ExposeAll" );
    b->setText( i18n("Toggle Present Windows (All desktops)" ));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));

    borderActivate = (ElectricBorder)conf.readEntry("BorderActivate", (int)ElectricNone);
    borderActivateAll = (ElectricBorder)conf.readEntry("BorderActivateAll", (int)ElectricTopLeft);
    layoutMode = conf.readEntry( "LayoutMode", int( LayoutNatural ));
    drawWindowCaptions = conf.readEntry("DrawWindowCaptions", true);
    drawWindowIcons = conf.readEntry("DrawWindowIcons", true);
    tabBox = conf.readEntry("TabBox", false);
    accuracy = conf.readEntry("Accuracy", 1) * 20;
    fillGaps = conf.readEntry("FillGaps", true);

    effects->reserveElectricBorder( borderActivate );
    effects->reserveElectricBorder( borderActivateAll );

    mActiveness.setCurveShape( TimeLine::EaseInOutCurve );
    mActiveness.setDuration( animationTime( conf, "RearrangeDuration", 250 ));
    mRearranging.setCurveShape( TimeLine::EaseInOutCurve );
    mRearranging.setDuration( animationTime( conf, "RearrangeDuration", 250 ));
    mRearranging.setProgress( 1.0 );
    highlightChangeTime = double( animationTime( 150 ));
    }

PresentWindowsEffect::~PresentWindowsEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    effects->unreserveElectricBorder( borderActivateAll );
    discardFilterTexture();
    }


void PresentWindowsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if(mActivated)
        {
        mActiveness.addTime(time);
        mRearranging.addTime(time);
        }
    else if(mActiveness.value() > 0.0)
        {
        mActiveness.removeTime(time);
        if(mActiveness.value() <= 0.0)
            effectTerminated();
        }

    // We need to mark the screen windows as transformed. Otherwise the whole
    //  screen won't be repainted, resulting in artefacts
    if( mActiveness.value() > 0.0 )
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void PresentWindowsEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mActiveness.value() > 0.0 )
        {
        if( mWindowData.contains(w) )
            {
            // This window will be transformed by the effect
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            // If it's minimized window or on another desktop and effect is not
            //  fully active, then apply some transparency
            if( mActiveness.value() < 1.0 && (w->isMinimized() || !w->isOnCurrentDesktop() ))
                data.setTranslucent();
            // Change window's highlight
            WindowData& windata = mWindowData[w];
            if( w == mHighlightedWindow )
                windata.highlight = qMin(1.0, windata.highlight + time / highlightChangeTime);
            else
                windata.highlight = qMax(0.0, windata.highlight - time / highlightChangeTime);
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
    if(mActiveness.value() > 0.0 && mWindowData.contains(w))
        {
        // Change window's position and scale
        const WindowData& windata = mWindowData[w];
        if( mRearranging.value() < 1.0 ) // rearranging
            {
            if( windata.old_area.isEmpty()) // no old position
                {
                data.xScale = windata.scale;
                data.yScale = windata.scale;
                data.xTranslate = windata.area.left() - w->x();
                data.yTranslate = windata.area.top() - w->y();
                data.opacity *= interpolate(0.0, 1.0, mRearranging.value());
                }
            else
                {
                data.xScale = interpolate(windata.old_scale, windata.scale, mRearranging.value());
                data.yScale = interpolate(windata.old_scale, windata.scale, mRearranging.value());
                data.xTranslate = (int)interpolate(windata.old_area.left() - w->x(),
                    windata.area.left() - w->x(), mRearranging.value());
                data.yTranslate = (int)interpolate(windata.old_area.top() - w->y(),
                    windata.area.top() - w->y(), mRearranging.value());
                }
            }
        else
            {
            data.xScale = interpolate(data.xScale, windata.scale, mActiveness.value());
            data.yScale = interpolate(data.yScale, windata.scale, mActiveness.value());
            data.xTranslate = (int)interpolate(data.xTranslate, windata.area.left() - w->x(), mActiveness.value());
            data.yTranslate = (int)interpolate(data.yTranslate, windata.area.top() - w->y(), mActiveness.value());
            }
        // Darken all windows except for the one under the cursor
        data.brightness *= interpolate(1.0, 0.7, mActiveness.value() * (1.0f - windata.highlight));
        // If it's minimized window or on another desktop and effect is not
        //  fully active, then apply some transparency
        if( mActiveness.value() < 1.0 && (w->isMinimized() || !w->isOnCurrentDesktop() ))
            data.opacity *= interpolate(0.0, 1.0, mActiveness.value());
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );

    if(mActiveness.value() > 0.0 && mWindowData.contains(w))
        {
        const WindowData& windata = mWindowData[w];
        if (drawWindowIcons)
            paintWindowIcon( w, data );

        if (drawWindowCaptions)
            {
            QString text = w->caption();
            QRect textArea( w->x() + data.xTranslate, w->y() + data.yTranslate,
                w->width() * data.xScale, w->height() * data.yScale );
            textArea.adjust( 10, 10, -10, -10 );
            double opacity = (0.7 + 0.2*windata.highlight) * data.opacity * mActiveness.value();
            QColor textcolor( 255, 255, 255, (int)(255*opacity) );
            QColor bgcolor( 0, 0, 0, (int)(255*opacity) );
            QFont f;
            f.setBold( true );
            f.setPointSize( 12 );
            effects->paintTextWithBackground( text, textArea, textcolor, bgcolor, f );
            }
        }
    }

void PresentWindowsEffect::postPaintScreen()
    {
    if( mActivated && mActiveness.value() < 1.0 ) // activating effect
        effects->addRepaintFull();
    if( mActivated && mRearranging.value() < 1.0 ) // rearranging
        effects->addRepaintFull();
    if( !mActivated && mActiveness.value() > 0.0 ) // deactivating effect
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

void PresentWindowsEffect::windowAdded( EffectWindow* w )
    {
    if( w->isSpecialWindow() || w->isUtility() )
        return;
    mWindowsToPresent.append( w );
    rearrangeWindows();
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
    if( mTabBoxMode && mActivated )
        {
        effects->closeTabBox();
        return;
        }
    mActivated = active;
    if( mActivated )
        {
        mWindowData.clear();
        windowFilter.clear();
        mWindowsToPresent.clear();
        const EffectWindowList& originalwindowlist = effects->stackingOrder();
        if( mTabBoxMode )
            {
            EffectWindowList tabBoxWindows = effects->currentTabBoxWindowList();
            int selectedWindow = tabBoxWindows.indexOf( effects->currentTabBoxWindow() );
            for( int i=selectedWindow; i<tabBoxWindows.count(); i++ )
                mWindowsToPresent.append( tabBoxWindows[ i ] );
            for( int i=selectedWindow-1; i>=0; i-- )
                mWindowsToPresent.append( tabBoxWindows[ i ] );
            }
        else
            {
            // Filter out special windows such as panels and taskbars
            foreach( EffectWindow* window, originalwindowlist )
                {
                if( window->isSpecialWindow() || window->isUtility() )
                    continue;
                if( window->isDeleted())
                    continue;
                if( !mShowWindowsFromAllDesktops && !window->isOnCurrentDesktop() )
                    continue;
                mWindowsToPresent.append(window);
                }
            }
        if( mWindowsToPresent.isEmpty())
            {
            mActivated = false; // don't activate with nothing to show
            return;
            }
        mActiveness.setProgress(0.0);
        effectActivated();
        rearrangeWindows();
        if( mTabBoxMode )
            setHighlightedWindow( effects->currentTabBoxWindow() );
        else
            setHighlightedWindow( effects->activeWindow() );
        }
    else
        {
        mWindowsToPresent.clear();
        mRearranging.setProgress(1.0); // turn off
        mActiveness.setProgress(1.0); // go back from arranged position
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

    screenGridSizes.clear();
    for( int i = 0; i < effects->numScreens(); i++ )
        screenGridSizes.append( GridSize() );
    numOfWindows.fill( 0, effects->numScreens() );
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
    QVector<EffectWindowList> windowlists;
    for( int i = 0; i < effects->numScreens(); i++ )
        windowlists.append( EffectWindowList() );

    if( windowFilter.isEmpty())
        {
        windowlist = mWindowsToPresent;
        foreach( EffectWindow* w, mWindowsToPresent )
            windowlists[ w->screen() ].append( w );
        }
    else
        {
        foreach( EffectWindow* w, mWindowsToPresent )
            {
            if( w->caption().contains( windowFilter, Qt::CaseInsensitive )
                || w->windowClass().contains( windowFilter, Qt::CaseInsensitive )
                || w->windowRole().contains( windowFilter, Qt::CaseInsensitive ))
                {
                windowlist.append( w );
                windowlists[ w->screen() ].append( w );
                }
            }
        }
    if( windowlist.isEmpty())
        {
        mWindowData.clear();
        setHighlightedWindow( NULL );
        effects->addRepaintFull();
        return;
        }

    // Check for changes if not the first arranging
    bool firstTime = false;
    if( !mWindowData.isEmpty())
        {
        DataHash newdata;
        for( DataHash::ConstIterator it = mWindowData.begin();
             it != mWindowData.end();
             ++it )
            if( windowlist.contains( it.key())) // remove windows that are not in the window list
                newdata[ it.key() ] = *it;
        mWindowData = newdata;
        // Initialize new entries
        foreach( EffectWindow* w, windowlist )
            if( !mWindowData.contains( w ))
                mWindowData[ w ].highlight = 0;
        }
    else
        firstTime = true;

    bool rearranging = false;
    int iterations = mTabBoxMode ? 1 : effects->numScreens(); // Only use one screen for window switching
    QVector<int> newNumOfWindows( effects->numScreens(), 0 );
    QVector<GridSize> newScreenGridSizes( effects->numScreens() );
    for( int i = 0; i < iterations; i++ )
        {
        int screen;
        EffectWindowList screenList;
        if( mTabBoxMode )
            {
            screen = effects->activeScreen();
            screenList = windowlist;
            }
        else
            {
            screen = i;
            screenList = windowlists[i];
            }

        newScreenGridSizes.append( GridSize() );

        // Do not rearrange if filtering only removed windows, so that the remaining ones don't possibly
        // jump into the freed slots if they'd be a better match. This only works with the regular grid
        // This can probably still lead to such things when removing the filter again, but that'd need
        // more complex remembering of window positions.
        bool doRearrange = false;
        if( layoutMode != LayoutRegularGrid )
            doRearrange = true;
        else
            {
            newNumOfWindows[screen] = screenList.count();
            newScreenGridSizes[screen].columns = int( ceil( sqrt( (double)screenList.count())));
            newScreenGridSizes[screen].rows = int( ceil( screenList.count() / double( newScreenGridSizes[screen].columns )));
            if( newNumOfWindows[screen] && ( firstTime || newNumOfWindows[screen] > numOfWindows[screen] ||
              ( newNumOfWindows[screen] < numOfWindows[screen] && ( newScreenGridSizes[screen].rows != screenGridSizes[screen].rows ||
                newScreenGridSizes[screen].columns != screenGridSizes[screen].columns ))))
                doRearrange = true;
            }
        if( doRearrange )
            {
            if( !firstTime && !rearranging )
                {
                rearranging = true;
                prepareToRearrange();
                }
            // No point calculating if there is no windows
            if( !screenList.size() )
                continue;
            // Calculate new positions and scales for windows
            if( layoutMode == LayoutRegularGrid || mTabBoxMode ) // Force the grid for window switching
                calculateWindowTransformationsClosest( screenList, screen );
            else if( layoutMode == LayoutFlexibleGrid )
                calculateWindowTransformationsKompose( screenList, screen );
            else
                calculateWindowTransformationsNatural( screenList, screen );
            }
        }

    numOfWindows = newNumOfWindows;
    screenGridSizes = newScreenGridSizes;

    if( !mWindowData.isEmpty() && mHighlightedWindow == NULL && !mTabBoxMode ) // Tab box takes care of this itself
        setHighlightedWindow( findFirstWindow());

    // Schedule entire desktop to be repainted
    effects->addRepaintFull();
    }

void PresentWindowsEffect::prepareToRearrange()
    {
    if( mHighlightedWindow != NULL && !mWindowData.contains( mHighlightedWindow ))
        setHighlightedWindow( NULL );
    for( DataHash::Iterator it = mWindowData.begin();
         it != mWindowData.end();
         ++it )
        {
        (*it).old_area = (*it).area;
        (*it).old_scale = (*it).scale;
        }
    mRearranging.setProgress(0.0); // start animation again
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

void PresentWindowsEffect::calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen)
    {
    QRect availRect = effects->clientArea( ScreenArea, screen, effects->currentDesktop());
    qSort( windowlist ); // The location of the windows should not depend on the stacking order

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
    //kDebug(1212) << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients";

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
                // Don't upscale large-ish windows
                if( widgetw > window->width() && ( window->width() > 300 || window->height() > 300 ))
                    {
                    widgetw = window->width();
                    widgeth = window->height();
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

            //kDebug(1212) << "Window '" << window->caption() << "' gets moved to (" <<
            //        mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            //        "), scale: " << mWindowData[window].scale << endl;
            }
        if ( maxRowHeights[i]-h > 0 )
            topOffset += maxRowHeights[i]-h;
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen)
    {
    QRect area = effects->clientArea( ScreenArea, screen, effects->currentDesktop());
    int columns = int( ceil( sqrt( (double)windowlist.count())));
    int rows = int( ceil( windowlist.count() / double( columns )));
    foreach( EffectWindow* w, windowlist )
        mWindowData[ w ].slot = -1;
    for(;;)
        {
        // Assign each window to the closest available slot
        assignSlots( windowlist, area, columns, rows );
        // Leave only the closest window in each slot, remove further conflicts
        getBestAssignments( windowlist );
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
    foreach( EffectWindow* w, windowlist )
        {
        WindowData *windowData = &mWindowData[ w ];
        QRect geom( area.x() + (windowData->slot % columns ) * slotwidth,
            area.y() + (windowData->slot / columns ) * slotheight,
            slotwidth, slotheight );
        geom.adjust( 10, 10, -10, -10 ); // borders
        double scale;
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
        if( scale > 2.0 || ( scale > 1.0 && ( w->width() > 300 || w->height() > 300 )))
        {
            scale = ( w->width() > 300 || w->height() > 300 ) ? 1.0 : 2.0;
            geom = QRect( geom.center().x() - int( w->width() * scale ) / 2, geom.center().y() - int( w->height() * scale ) / 2,
                          scale * w->width(), scale * w->height() );
        }
        windowData->area = geom;
        windowData->scale = scale;
        }
    }

void PresentWindowsEffect::assignSlots( EffectWindowList windowlist, const QRect& area, int columns, int rows )
    {
    QVector< bool > taken;
    taken.fill( false, columns * rows );
    foreach( EffectWindow* w, windowlist )
        {
        if( mWindowData[ w ].slot != -1 )
            taken[ mWindowData[ w ].slot ] = true;
        }
    int slotwidth = area.width() / columns;
    int slotheight = area.height() / rows;
    if( mTabBoxMode )
        {
        for( int i=0; i<windowlist.count(); i++ )
            {
            EffectWindow *w = windowlist[ i ];
            WindowData *windowData = &mWindowData[ w ];
            if( windowData->slot != -1 )
                continue; // it already has a slot
            int x = i%columns;
            int y = i/columns;
            QPoint pos = w->geometry().center();
            if( pos.x() < area.left())
                pos.setX( area.left());
            if( pos.x() > area.right())
                pos.setX( area.right());
            if( pos.y() < area.top())
                pos.setY( area.top());
            if( pos.y() > area.bottom())
                pos.setY( area.bottom());
            //int distance = INT_MAX;
            int xdiff = pos.x() - ( area.x() + slotwidth * x + slotwidth / 2 ); // slotwidth/2 for center
            int ydiff = pos.y() - ( area.y() + slotheight * y + slotheight / 2 );
            int dist = int( sqrt( (double)(xdiff * xdiff + ydiff * ydiff) ));
            windowData->slot = i;
            windowData->x = x;
            windowData->y = y;
            windowData->slot_distance = dist;
            }
        }
    else
        {
        foreach( EffectWindow* w, windowlist )
            {
            WindowData *windowData = &mWindowData[ w ];
            if( windowData->slot != -1 )
                continue; // it already has a slot
            QPoint pos = w->geometry().center();
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
                        windowData->slot = slot;
                        windowData->x = x;
                        windowData->y = y;
                        windowData->slot_distance = distance;
                        }
                    }
            }
        }
    }

void PresentWindowsEffect::getBestAssignments( EffectWindowList windowlist )
    {
    foreach( EffectWindow* w1, windowlist )
        {
        WindowData *windowData1 = &mWindowData[ w1 ];
        foreach( EffectWindow* w2, windowlist )
            {
            WindowData *windowData2 = &mWindowData[ w2 ];
            if( w1 != w2 && windowData1->slot == windowData2->slot
                && windowData1->slot_distance >= windowData2->slot_distance )
                {
                windowData1->slot = -1;
                }
            }
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen)
    {
    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    qSort( windowlist );

    QRect area = effects->clientArea( ScreenArea, screen, effects->currentDesktop());
    QRect bounds = area;//mWindowData[0].area;
    int direction = 0;
    foreach( EffectWindow* w, windowlist )
        {
        bounds = bounds.united( w->geometry() );
        mWindowData[ w ].area = w->geometry();
        mWindowData[ w ].scale = 1.0;
        // Reuse the unused "slot" as a preferred direction attribute. This is used when the window
        // is on the edge of the screen to try to use as much screen real estate as possible.
        mWindowData[ w ].slot = direction;
        direction++;
        if( direction == 4 )
            direction = 0;
        }
    
    // Iterate over all windows, if two overlap push them apart _slightly_ as we try to
    // brute-force the most optimal positions over many iterations.
    bool overlap;
    do
        {
        overlap = false;
        foreach( EffectWindow* w, windowlist )
            {
            foreach( EffectWindow* e, windowlist )
                {
                if( w != e && mWindowData[ w ].area.adjusted( -5, -5, 5, 5 ).intersects(
                              mWindowData[ e ].area.adjusted( -5, -5, 5, 5 )))
                    {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff( mWindowData[ e ].area.center() - mWindowData[ w ].area.center() );
                    // Prevent dividing by zero and non-movement
                    if( diff.x() == 0 && diff.y() == 0 )
                        diff.setX( 1 );
                    // Try to keep screen aspect ratio
                    //if( bounds.height() / bounds.width() > area.height() / area.width() )
                    //    diff.setY( diff.y() / 2 );
                    //else
                    //    diff.setX( diff.x() / 2 );
                    // Approximate a vector of between 10px and 20px in magnitude in the same direction
                    diff *= accuracy / double( diff.manhattanLength() );
                    // Move both windows apart
                    mWindowData[ w ].area.translate( -diff );
                    mWindowData[ e ].area.translate( diff );

                    // Try to keep the bounding rect the same aspect as the screen so that more
                    // screen real estate is utilised. We do this by splitting the screen into nine
                    // equal sections, if the window center is in any of the corner sections pull the
                    // window towards the outer corner. If it is in any of the other edge sections
                    // alternate between each corner on that edge. We don't want to determine it
                    // randomly as it will not produce consistant locations when using the filter.
                    // Only move one window so we don't cause large amounts of unnecessary zooming
                    // in some situations. We need to do this even when expanding later just in case
                    // all windows are the same size.
                    // (We are using an old bounding rect for this, hopefully it doesn't matter)
                    int xSection = ( mWindowData[ w ].area.x() - bounds.x() ) / ( bounds.width() / 3 );
                    int ySection = ( mWindowData[ w ].area.y() - bounds.y() ) / ( bounds.height() / 3 );
                    diff = QPoint( 0, 0 );
                    if( xSection != 1 || ySection != 1 ) // Remove this if you want the center to pull as well
                        {
                        if( xSection == 1 )
                            xSection = ( mWindowData[ w ].slot / 2 ? 2 : 0 );
                        if( ySection == 1 )
                            ySection = ( mWindowData[ w ].slot % 2 ? 2 : 0 );
                        }
                    if( xSection == 0 && ySection == 0 )
                        diff = QPoint( bounds.topLeft() - mWindowData[ w ].area.center() );
                    if( xSection == 2 && ySection == 0 )
                        diff = QPoint( bounds.topRight() - mWindowData[ w ].area.center() );
                    if( xSection == 2 && ySection == 2 )
                        diff = QPoint( bounds.bottomRight() - mWindowData[ w ].area.center() );
                    if( xSection == 0 && ySection == 2 )
                        diff = QPoint( bounds.bottomLeft() - mWindowData[ w ].area.center() );
                    if( diff.x() != 0 || diff.y() != 0 )
                        {
                        diff *= accuracy / double( diff.manhattanLength() );
                        mWindowData[ w ].area.translate( diff );
                        }
    
                    // Update bounding rect
                    bounds = bounds.united( mWindowData[ w ].area );
                    bounds = bounds.united( mWindowData[ e ].area );
                    }
                }
            }
        } while( overlap );

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    // The 20's and 10's are so that the windows don't touch the edge of the screen.
    double scale;
    if( bounds == area )
        scale = 1.0; // Don't add borders to the screen
    else if( area.width() / double( bounds.width() ) < area.height() / double( bounds.height() ))
        scale = ( area.width() - 20 ) / double( bounds.width() );
    else
        scale = ( area.height() - 20 ) / double( bounds.height() );
    // Make bounding rect fill the screen size for later steps
    bounds = QRect(
        bounds.x() - ( area.width() - 20 - bounds.width() * scale ) / 2 - 10 / scale,
        bounds.y() - ( area.height() - 20 - bounds.height() * scale ) / 2 - 10 / scale,
        area.width() / scale,
        area.height() / scale
        );

    // Move all windows back onto the screen and set their scale
    foreach( EffectWindow* w, windowlist )
        {
        mWindowData[ w ].scale = scale;
        mWindowData[ w ].area = QRect(
            ( mWindowData[ w ].area.x() - bounds.x() ) * scale + area.x(),
            ( mWindowData[ w ].area.y() - bounds.y() ) * scale + area.y(),
            mWindowData[ w ].area.width() * scale,
            mWindowData[ w ].area.height() * scale
            );
        }

    // Try to fill the gaps by enlarging windows if they have the space
    if( fillGaps )
        {
        // Don't expand onto or over the border
        QRegion borderRegion( area.adjusted( -200, -200, 200, 200 ));
        borderRegion ^= area.adjusted( 10 / scale, 10 / scale, -10 / scale, -10 / scale );
        
        bool moved;
        do
            {
            moved = false;
            foreach( EffectWindow* w, windowlist )
                {
                QRect oldRect;
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = accuracy;
                int heightDiff = windowHeightForWidth( w, mWindowData[ w ].area.width() + widthDiff ) - mWindowData[ w ].area.height();
                int xDiff = widthDiff / 2;  // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.
        
                // Attempt enlarging to the top-right
                oldRect = mWindowData[ w ].area;
                mWindowData[ w ].area = QRect(
                    mWindowData[ w ].area.x() + xDiff,
                    mWindowData[ w ].area.y() - yDiff - heightDiff,
                    mWindowData[ w ].area.width() + widthDiff,
                    mWindowData[ w ].area.height() + heightDiff
                    );
                if( isOverlappingAny( w, windowlist, borderRegion ))
                    mWindowData[ w ].area = oldRect;
                else
                    {
                    mWindowData[ w ].scale = mWindowData[ w ].area.width() / double( w->width() );
                    moved = true;
                    }
                
                // Attempt enlarging to the bottom-right
                oldRect = mWindowData[ w ].area;
                mWindowData[ w ].area = QRect(
                    mWindowData[ w ].area.x() + xDiff,
                    mWindowData[ w ].area.y() + yDiff,
                    mWindowData[ w ].area.width() + widthDiff,
                    mWindowData[ w ].area.height() + heightDiff
                    );
                if( isOverlappingAny( w, windowlist, borderRegion ))
                    mWindowData[ w ].area = oldRect;
                else
                    {
                    mWindowData[ w ].scale = mWindowData[ w ].area.width() / double( w->width() );
                    moved = true;
                    }
                
                // Attempt enlarging to the bottom-left
                oldRect = mWindowData[ w ].area;
                mWindowData[ w ].area = QRect(
                    mWindowData[ w ].area.x() - xDiff - widthDiff,
                    mWindowData[ w ].area.y() + yDiff,
                    mWindowData[ w ].area.width() + widthDiff,
                    mWindowData[ w ].area.height() + heightDiff
                    );
                if( isOverlappingAny( w, windowlist, borderRegion ))
                    mWindowData[ w ].area = oldRect;
                else
                    {
                    mWindowData[ w ].scale = mWindowData[ w ].area.width() / double( w->width() );
                    moved = true;
                    }
                
                // Attempt enlarging to the top-left
                oldRect = mWindowData[ w ].area;
                mWindowData[ w ].area = QRect(
                    mWindowData[ w ].area.x() - xDiff - widthDiff,
                    mWindowData[ w ].area.y() - yDiff - heightDiff,
                    mWindowData[ w ].area.width() + widthDiff,
                    mWindowData[ w ].area.height() + heightDiff
                    );
                if( isOverlappingAny( w, windowlist, borderRegion ))
                    mWindowData[ w ].area = oldRect;
                else
                    {
                    mWindowData[ w ].scale = mWindowData[ w ].area.width() / double( w->width() );
                    moved = true;
                    }
                }
            } while( moved );

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        foreach( EffectWindow* w, windowlist )
            {
            if( mWindowData[ w ].scale > 2.0 ||
              ( mWindowData[ w ].scale > 1.0 && ( w->width() > 300 || w->height() > 300 )))
                {
                    mWindowData[ w ].scale = ( w->width() > 300 || w->height() > 300 ) ? 1.0 : 2.0;
                    mWindowData[ w ].area = QRect(
                        mWindowData[ w ].area.center().x() - int( w->width() * mWindowData[ w ].scale ) / 2,
                        mWindowData[ w ].area.center().y() - int( w->height() * mWindowData[ w ].scale ) / 2,
                        w->width() * mWindowData[ w ].scale,
                        w->height() * mWindowData[ w ].scale
                        );
                }
            }
        }
    }

bool PresentWindowsEffect::isOverlappingAny( EffectWindow* w, const EffectWindowList& windowlist, const QRegion& border )
    {
    if( border.intersects( mWindowData[ w ].area ))
        return true;
    // Is there a better way to do this?
    foreach( EffectWindow* e, windowlist )
        {
        if( w == e )
            continue;
        if( mWindowData[ w ].area.adjusted( -5, -5, 5, 5 ).intersects(
            mWindowData[ e ].area.adjusted( -5, -5, 5, 5 )))
            return true;
        }
    return false;
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
    if( mTabBoxMode )
        effects->setTabBoxWindow( w );
    }

// returns a window which is to relative position <xdiff,ydiff> from the given window
EffectWindow* PresentWindowsEffect::relativeWindow( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const
    {
    // Using the grid to determine the window only works on grid layouts and only have one screen.
    // On multi-screen setups the grid coords are reused on each screen and each screen may also
    // have a different grid size.
    if( layoutMode != LayoutNatural && effects->numScreens() < 2 )
        return relativeWindowGrid( w, xdiff, ydiff, wrap );

    // Attempt to find the window from its rect instead
    EffectWindow* next;
    QRect area = effects->clientArea( FullArea, 0, effects->currentDesktop() );
    QRect detectRect;

    // Detect across the width of the desktop
    if( xdiff != 0 )
        {
        if( xdiff > 0 )
            { // Detect right
            for( int i = 0; i < xdiff; i++ )
                {
                detectRect = QRect( 0, mWindowData[ w ].area.y(), area.width(), mWindowData[ w ].area.height() );
                next = NULL;
                foreach( EffectWindow* e, mWindowsToPresent )
                    {
                    if( mWindowData[ e ].area.intersects( detectRect ) &&
                        mWindowData[ e ].area.x() > mWindowData[ w ].area.x() )
                        {
                        if( next == NULL )
                            next = e;
                        else if( mWindowData[ e ].area.x() < mWindowData[ next ].area.x() )
                            next = e;
                        }
                    }
                if( next == NULL )
                    {
                    if( wrap ) // We are at the right-most window, now get the left-most one to wrap
                        return relativeWindow( w, -1000, 0, false );
                    break; // No more windows to the right
                    }
                w = next;
                }
            return w;
            }
        else
            { // Detect left
            for( int i = 0; i < -xdiff; i++ )
                {
                detectRect = QRect( 0, mWindowData[ w ].area.y(), area.width(), mWindowData[ w ].area.height() );
                next = NULL;
                foreach( EffectWindow* e, mWindowsToPresent )
                    {
                    if( mWindowData[ e ].area.intersects( detectRect ) &&
                        mWindowData[ e ].area.x() + mWindowData[ e ].area.width() < mWindowData[ w ].area.x() + mWindowData[ w ].area.width() )
                        {
                        if( next == NULL )
                            next = e;
                        else if( mWindowData[ e ].area.x() + mWindowData[ e ].area.width() > mWindowData[ next ].area.x() + mWindowData[ next ].area.width() )
                            next = e;
                        }
                    }
                if( next == NULL )
                    {
                    if( wrap ) // We are at the left-most window, now get the right-most one to wrap
                        return relativeWindow( w, 1000, 0, false );
                    break; // No more windows to the left
                    }
                w = next;
                }
            return w;
            }
        }

    // Detect across the height of the desktop
    if( ydiff != 0 )
        {
        if( ydiff > 0 )
            { // Detect down
            for( int i = 0; i < ydiff; i++ )
                {
                detectRect = QRect( mWindowData[ w ].area.x(), 0, mWindowData[ w ].area.width(), area.height() );
                next = NULL;
                foreach( EffectWindow* e, mWindowsToPresent )
                    {
                    if( mWindowData[ e ].area.intersects( detectRect ) &&
                        mWindowData[ e ].area.y() > mWindowData[ w ].area.y() )
                        {
                        if( next == NULL )
                            next = e;
                        else if( mWindowData[ e ].area.y() < mWindowData[ next ].area.y() )
                            next = e;
                        }
                    }
                if( next == NULL )
                    {
                    if( wrap ) // We are at the bottom-most window, now get the top-most one to wrap
                        return relativeWindow( w, 0, -1000, false );
                    break; // No more windows to the bottom
                    }
                w = next;
                }
            return w;
            }
        else
            { // Detect up
            for( int i = 0; i < -ydiff; i++ )
                {
                detectRect = QRect( mWindowData[ w ].area.x(), 0, mWindowData[ w ].area.width(), area.height() );
                next = NULL;
                foreach( EffectWindow* e, mWindowsToPresent )
                    {
                    if( mWindowData[ e ].area.intersects( detectRect ) &&
                        mWindowData[ e ].area.y() + mWindowData[ e ].area.height() < mWindowData[ w ].area.y() + mWindowData[ w ].area.height() )
                        {
                        if( next == NULL )
                            next = e;
                        else if( mWindowData[ e ].area.y() + mWindowData[ e ].area.height() > mWindowData[ next ].area.y() + mWindowData[ next ].area.height() )
                            next = e;
                        }
                    }
                if( next == NULL )
                    {
                    if( wrap ) // We are at the top-most window, now get the bottom-most one to wrap
                        return relativeWindow( w, 0, 1000, false );
                    break; // No more windows to the top
                    }
                w = next;
                }
            return w;
            }
        }

    assert( false ); // Should never get here
    }

EffectWindow* PresentWindowsEffect::relativeWindowGrid( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const
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
    if( layoutMode == LayoutNatural || effects->numScreens() > 1 )
        {
        EffectWindow* topLeft = NULL;
        foreach( EffectWindow* w, mWindowsToPresent )
            {
            if( topLeft == NULL )
                topLeft = w;
            else if( w->x() < topLeft->x() || w->y() < topLeft->y() )
                topLeft = w;
            }
        return topLeft;
        }

    // The following only works on grid layouts on a single screen

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
    if( data.icon.cacheKey() != w->icon().cacheKey())
        { // make sure data.icon is the right QPixmap, and rebind
        data.icon = w->icon();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing )
            {
            data.iconTexture = new GLTexture( data.icon );
            data.iconTexture->setFilter( GL_LINEAR );
            }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing )
            data.iconPicture = XRenderPicture( data.icon );
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
        glColor4f( 0, 0, 0, 0.5 * mActiveness.value() );
        renderRoundBox( QRect( x-3, y-3, width+6, height+6 ), 3 );
        // Render the icon
        glColor4f( 1, 1, 1, 1 * mActiveness.value() );
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        data.iconTexture->bind();
        data.iconTexture->render( infiniteRegion(), QRect( x, y, width, height ));
        data.iconTexture->unbind();
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

void PresentWindowsEffect::tabBoxAdded( int mode )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( mActivated )
        return;
    if( !tabBox )
        return;
    if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
        {
        mTabBoxMode = true;
        setActive( true );
        if( mActivated )
            effects->refTabBox();
        }
    }

void PresentWindowsEffect::tabBoxClosed()
    {
    if( mActivated )
        {
        mTabBoxMode = false;
        effects->unrefTabBox();
        setActive( false );
        }
    }

void PresentWindowsEffect::tabBoxUpdated()
    {
    if( mActivated )
        setHighlightedWindow( effects->currentTabBoxWindow() );
    }

} // namespace
#include "presentwindows.moc"
