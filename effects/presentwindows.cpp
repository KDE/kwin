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
    : m_borderActivate( ElectricNone )
    , m_borderActivateAll( ElectricNone )
    ,   m_activated( false )
    ,   m_allDesktops( false )
    ,   m_ignoreMinimized( false )
    ,   m_decalOpacity( 0.0 )
    //,   m_input()
    ,   m_hasKeyboardGrab( false )
    ,   m_tabBoxEnabled( false )
    //,   m_motionManager()
    //,   m_windowData()
    ,   m_highlightedWindow( NULL )
    //,   m_gridSizes()
    //,   m_windowFilter()
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    ,   m_filterTexture( NULL )
    //,   m_filterTextureRect()
    //,   m_filterFrameRect()
#endif
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = ( KAction* )actionCollection->addAction( "Expose" );
    a->setText( i18n( "Toggle Present Windows (Current desktop)" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F9 ));
    connect( a, SIGNAL( triggered(bool) ), this, SLOT( toggleActive() ));
    KAction* b = ( KAction* )actionCollection->addAction( "ExposeAll" );
    b->setText( i18n( "Toggle Present Windows (All desktops)" ));
    b->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F10 ));
    connect( b, SIGNAL( triggered(bool) ), this, SLOT( toggleActiveAllDesktops() ));
    reconfigure( ReconfigureAll );
    }

PresentWindowsEffect::~PresentWindowsEffect()
    {
    effects->unreserveElectricBorder( m_borderActivate );
    effects->unreserveElectricBorder( m_borderActivateAll );
    discardFilterTexture();
    }

void PresentWindowsEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig("PresentWindows");
    effects->unreserveElectricBorder( m_borderActivate );
    effects->unreserveElectricBorder( m_borderActivateAll );
    m_borderActivate = ElectricBorder( conf.readEntry( "BorderActivate", int( ElectricNone )));
    m_borderActivateAll = ElectricBorder( conf.readEntry( "BorderActivateAll", int( ElectricTopLeft )));
    effects->reserveElectricBorder( m_borderActivate );
    effects->reserveElectricBorder( m_borderActivateAll );
    m_layoutMode = conf.readEntry( "LayoutMode", int( LayoutNatural ));
    m_showCaptions = conf.readEntry( "DrawWindowCaptions", true );
    m_showIcons = conf.readEntry( "DrawWindowIcons", true );
    m_tabBoxAllowed = conf.readEntry( "TabBox", false );
    m_ignoreMinimized = conf.readEntry( "IgnoreMinimized", false );
    m_accuracy = conf.readEntry( "Accuracy", 1 ) * 20;
    m_fillGaps = conf.readEntry( "FillGaps", true );
    m_fadeDuration = double( animationTime( 150 ));
    }

//-----------------------------------------------------------------------------
// Screen painting

void PresentWindowsEffect::prePaintScreen( ScreenPrePaintData &data, int time )
    {
    m_motionManager.calculate( time );

    // We need to mark the screen as having been transformed otherwise there will be no repainting
    if( m_activated || m_motionManager.managingWindows() )
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    if( m_activated )
        m_decalOpacity = qMin( 1.0, m_decalOpacity + time / m_fadeDuration );
    else
        m_decalOpacity = qMax( 0.0, m_decalOpacity - time / m_fadeDuration );

    effects->prePaintScreen( data, time );
    }

void PresentWindowsEffect::paintScreen( int mask, QRegion region, ScreenPaintData &data )
    {
    effects->paintScreen( mask, region, data );

    // Display the filter box
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( m_filterTexture && region.intersects( m_filterFrameRect ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        // First render the frame
        QColor color = QPalette().color( QPalette::Active, QPalette::Highlight );
        glColor4f( color.redF(), color.greenF(), color.blueF(), 0.75f );
        renderRoundBoxWithEdge( m_filterFrameRect );

        // Then the text on top of it
        m_filterTexture->bind();
        m_filterTexture->render( region, m_filterTextureRect );
        m_filterTexture->unbind();
        glPopAttrib();
        }
#endif
    }

void PresentWindowsEffect::postPaintScreen()
    {
    if( m_motionManager.areWindowsMoving() )
        effects->addRepaintFull();
    else if( !m_activated && m_motionManager.managingWindows() )
        { // We have finished moving them back, stop processing
        m_motionManager.unmanageAll();
        m_windowData.clear();
        effects->setActiveFullScreenEffect( NULL );
        }

    // Update windows that are changing brightness or opacity
    foreach( EffectWindow *w, m_windowData.keys() )
        {
        if( m_windowData[w].opacity > 0.0 && m_windowData[w].opacity < 1.0 )
            w->addRepaintFull();
        if( m_windowData[w].highlight > 0.0 && m_windowData[w].highlight < 1.0 )
            w->addRepaintFull();
        }
    
    effects->postPaintScreen();
    }

//-----------------------------------------------------------------------------
// Window painting

void PresentWindowsEffect::prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time )
    {
    // TODO: We should also check to see if any windows are fading just in case fading takes longer
    //       than moving the windows when the effect is deactivated.
    if( m_activated || m_motionManager.areWindowsMoving() )
        {
        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE ); // Display always
        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );

        // Calculate window's opacity
        // TODO: Minimized windows or windows not on the current desktop are only 75% visible?
        if( m_windowData[w].visible )
            m_windowData[w].opacity = qMin(/*( w->isMinimized() || !w->isOnCurrentDesktop() ) ? 0.75 :*/ 1.0,
                m_windowData[w].opacity + time / m_fadeDuration );
        else
            m_windowData[w].opacity = qMax( 0.0, m_windowData[w].opacity - time / m_fadeDuration );
        if( m_windowData[w].opacity == 0.0 )
            w->disablePainting( EffectWindow::PAINT_DISABLED );
        else if( m_windowData[w].opacity != 1.0 )
            data.setTranslucent();

        // Calculate window's brightness
        if( w == m_highlightedWindow || !m_activated )
            m_windowData[w].highlight = qMin( 1.0, m_windowData[w].highlight + time / m_fadeDuration );
        else
            m_windowData[w].highlight = qMax( 0.0, m_windowData[w].highlight - time / m_fadeDuration );

        if( m_motionManager.isManaging( w ))
            data.setTransformed(); // We will be moving this window
        }
    effects->prePaintWindow( w, data, time );
    }

void PresentWindowsEffect::paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data )
    {
    if( m_activated || m_motionManager.areWindowsMoving() )
        {
        // Apply opacity and brightness
        data.opacity *= m_windowData[w].opacity;
        data.brightness *= interpolate( 0.7, 1.0, m_windowData[w].highlight );

        if( m_motionManager.isManaging( w ))
            {
            m_motionManager.apply( w, data );

            effects->paintWindow( w, mask, region, data );
            
            const WindowData &wData = m_windowData[w];
            if( m_showIcons )
                paintWindowIcon( w, data );
            if( m_showCaptions )
                {
                QString text = w->caption();
                QRect textArea( w->x() + data.xTranslate, w->y() + data.yTranslate,
                    w->width() * data.xScale, w->height() * data.yScale );
                textArea.adjust( 10, 10, -10, -10 );
                double opacity = (0.7 + 0.2 * wData.highlight) * data.opacity * m_decalOpacity;
                QColor textcolor( 255, 255, 255, int( 255 * opacity ));
                QColor bgcolor( 0, 0, 0, int( 255 * opacity ));
                QFont f;
                f.setBold( true );
                f.setPointSize( 12 );
                effects->paintTextWithBackground( text, textArea, textcolor, bgcolor, f );
                }
            }
        else
            effects->paintWindow( w, mask, region, data );
        }
    else
        effects->paintWindow( w, mask, region, data );
    }

//-----------------------------------------------------------------------------
// User interaction

void PresentWindowsEffect::windowAdded( EffectWindow *w )
    {
    m_windowData[w].visible = isVisibleWindow( w );
    m_windowData[w].opacity = 0.0;
    m_windowData[w].highlight = 0.0;
    if( isSelectableWindow( w ))
        {
        m_motionManager.manage( w );
        rearrangeWindows();
        }
    }

void PresentWindowsEffect::windowClosed( EffectWindow *w )
    {
    if( m_highlightedWindow == w )
        setHighlightedWindow( findFirstWindow() );
    m_windowData[w].visible = false; // TODO: Fix this so they do actually fade out
    rearrangeWindows();
    }

void PresentWindowsEffect::windowDeleted( EffectWindow *w )
    {
    m_windowData.remove( w );
    m_motionManager.unmanage( w );
    }

bool PresentWindowsEffect::borderActivated( ElectricBorder border )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return false;
    if( border == m_borderActivate && !m_activated )
        {
        toggleActive();
        return true;
        }
    if( border == m_borderActivateAll && !m_activated )
        {
        toggleActiveAllDesktops();
        return true;
        }
    return false;
    }

void PresentWindowsEffect::windowInputMouseEvent( Window w, QEvent *e )
    {
    assert( w == m_input );

    // Which window are we hovering over? Always trigger as we don't always get move events before clicking
    // We cannot use m_motionManager.windowAtPoint() as the window might not be visible
    EffectWindowList windows = m_motionManager.managedWindows();
    for( int i = 0; i < windows.size(); i++ )
        {
        if( m_motionManager.transformedGeometry( windows.at( i )).contains( cursorPos() ) &&
            m_windowData[windows.at( i )].visible )
            {
            if( windows.at( i ) && m_highlightedWindow != windows.at( i ))
                setHighlightedWindow( windows.at( i ));
            break;
            }
        }

    if( e->type() != QEvent::MouseButtonPress )
        return;

    QMouseEvent* me = static_cast<QMouseEvent*>( e );
    if( me->button() == Qt::LeftButton )
        {
        if( m_highlightedWindow )
            effects->activateWindow( m_highlightedWindow );
        setActive( false );
        }
    // TODO: User mouse actions. E.g. right-click stickies and middle-click brings to current desktop
    }

void PresentWindowsEffect::grabbedKeyboardEvent( QKeyEvent *e )
    {
    if( e->type() == QEvent::KeyPress )
        {
        switch( e->key() )
            { // Wrap only if not auto-repeating
            case Qt::Key_Left:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, -1, 0, !e->isAutoRepeat() ));
                break;
            case Qt::Key_Right:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 1, 0, !e->isAutoRepeat() ));
                break;
            case Qt::Key_Up:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 0, -1, !e->isAutoRepeat() ));
                break;
            case Qt::Key_Down:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 0, 1, !e->isAutoRepeat() ));
                break;
            case Qt::Key_Home:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, -1000, 0, false ));
                break;
            case Qt::Key_End:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 1000, 0, false ));
                break;
            case Qt::Key_PageUp:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 0, -1000, false ));
                break;
            case Qt::Key_PageDown:
                setHighlightedWindow( relativeWindow( m_highlightedWindow, 0, 1000, false ));
                break;
            case Qt::Key_Backspace:
                if( !m_windowFilter.isEmpty() )
                    {
                    m_windowFilter.remove( m_windowFilter.length() - 1, 1 );
                    updateFilterTexture();
                    rearrangeWindows();
                    }
                return;
            case Qt::Key_Escape:
                setActive( false );
                return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if( m_highlightedWindow )
                    effects->activateWindow( m_highlightedWindow );
                setActive( false );
                return;
            case Qt::Key_Tab:
                return; // Nothing at the moment
            default:
                if( !e->text().isEmpty() )
                    {
                    m_windowFilter.append( e->text() );
                    updateFilterTexture();
                    rearrangeWindows();
                    return;
                    }
                break;
            }
        }
    }

//-----------------------------------------------------------------------------
// Tab box

void PresentWindowsEffect::tabBoxAdded( int mode )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( m_activated )
        return;
    if( !m_tabBoxAllowed )
        return;
    if( mode == TabBoxWindowsMode && effects->currentTabBoxWindowList().count() > 0 )
        {
        m_tabBoxEnabled = true;
        setActive( true );
        if( m_activated )
            effects->refTabBox();
        }
    }

void PresentWindowsEffect::tabBoxClosed()
    {
    if( m_activated )
        {
        m_tabBoxEnabled = false;
        effects->unrefTabBox();
        setActive( false );
        }
    }

void PresentWindowsEffect::tabBoxUpdated()
    {
    if( m_activated )
        setHighlightedWindow( effects->currentTabBoxWindow() );
    }

//-----------------------------------------------------------------------------
// Window rearranging

void PresentWindowsEffect::rearrangeWindows()
    {
    if( !m_activated )
        return;

    effects->addRepaintFull(); // Trigger the first repaint

    // Work out which windows are on which screens
    EffectWindowList windowlist;
    QList<EffectWindowList> windowlists;
    for( int i = 0; i < effects->numScreens(); i++ )
        windowlists.append( EffectWindowList() );

    if( m_windowFilter.isEmpty() )
        {
        if( m_tabBoxEnabled )
            // Assume we correctly set things up, should be identical to
            // m_motionManager except just in a slightly different order.
            windowlist = effects->currentTabBoxWindowList();
        else
            windowlist = m_motionManager.managedWindows();
        foreach( EffectWindow *w, m_motionManager.managedWindows() )
            {
            windowlists[w->screen()].append( w );
            m_windowData[w].visible = true;
            }
        }
    else
        { // Can we move this filtering somewhere else?
        foreach( EffectWindow *w, m_motionManager.managedWindows() )
            {
            if( w->caption().contains( m_windowFilter, Qt::CaseInsensitive ) ||
                w->windowClass().contains( m_windowFilter, Qt::CaseInsensitive ) ||
                w->windowRole().contains( m_windowFilter, Qt::CaseInsensitive ))
                {
                windowlist.append( w );
                windowlists[w->screen()].append( w );
                m_windowData[w].visible = true;
                }
            else
                m_windowData[w].visible = false;
            }
        }
    if( windowlist.isEmpty() )
        {
        setHighlightedWindow( NULL ); // TODO: Having a NULL highlighted window isn't really safe
        return;
        }

    // We filtered out the highlighted window
    if( m_highlightedWindow )
        {
        if( m_windowData[m_highlightedWindow].visible == false )
            setHighlightedWindow( findFirstWindow() );
        }
    else
        setHighlightedWindow( findFirstWindow() );

    int screens = m_tabBoxEnabled ? 1 : effects->numScreens();
    for( int screen = 0; screen < screens; screen++ )
        {
        EffectWindowList windows;
        if( m_tabBoxEnabled )
            { // Kind of cheating here
            screen = effects->activeScreen();
            windows = windowlist;
            }
        else
            windows = windowlists[screen];

        // Don't rearrange if the grid is the same size as what it was before to prevent
        // windows moving to a better spot if one was filtered out.
        if( m_layoutMode == LayoutRegularGrid &&
            m_gridSizes[screen].columns * m_gridSizes[screen].rows &&
            windows.size() < m_gridSizes[screen].columns * m_gridSizes[screen].rows &&
            windows.size() > ( m_gridSizes[screen].columns - 1) * m_gridSizes[screen].rows &&
            windows.size() > m_gridSizes[screen].columns * ( m_gridSizes[screen].rows - 1 ))
            continue;

        // No point continuing if there is no windows to process
        if( !windows.count() )
            continue;

        if( m_layoutMode == LayoutRegularGrid || m_tabBoxEnabled ) // Force the grid for window switching
            calculateWindowTransformationsClosest( windows, screen );
        else if( m_layoutMode == LayoutFlexibleGrid )
            calculateWindowTransformationsKompose( windows, screen );
        else
            calculateWindowTransformationsNatural( windows, screen );
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsClosest( EffectWindowList windowlist, int screen )
    {
    QRect area = effects->clientArea( ScreenArea, screen, effects->currentDesktop() );
    int columns = int( ceil( sqrt( double( windowlist.count() ))));
    int rows = int( ceil( windowlist.count() / double( columns )));

    // Remember the size for later
    m_gridSizes[screen].columns = columns;
    m_gridSizes[screen].rows = rows;

    // Assign slots
    foreach( EffectWindow *w, windowlist )
        m_windowData[w].slot = -1;
    if( m_tabBoxEnabled )
        { // Rearrange in the correct order. As rearrangeWindows() is only ever
          // called once so we can use effects->currentTabBoxWindow() here.
        int selectedWindow = qMax( 0, windowlist.indexOf( effects->currentTabBoxWindow() ));
        int slot = 0;
        for( int i = selectedWindow; i < windowlist.count(); i++ )
            m_windowData[windowlist[i]].slot = slot++;
        for( int i = selectedWindow - 1; i >= 0; i-- )
            m_windowData[windowlist[i]].slot = slot++;
        }
    else
        {
        for(;;)
            {
            // Assign each window to the closest available slot
            assignSlots( windowlist, area, columns, rows );
            // Leave only the closest window in each slot, remove further conflicts
            getBestAssignments( windowlist );
            bool allAssigned = true;
            foreach( EffectWindow *w, windowlist )
                if( m_windowData[w].slot == -1 )
                    {
                    allAssigned = false;
                    break;
                    }
            if( allAssigned )
                break;
            }
        }

    int slotWidth = area.width() / columns;
    int slotHeight = area.height() / rows;
    foreach( EffectWindow *w, windowlist )
        {
        assert( m_windowData[w].slot != -1 );

        // Work out where the slot is
        QRect target(
            area.x() + ( m_windowData[w].slot % columns ) * slotWidth,
            area.y() + ( m_windowData[w].slot / columns ) * slotHeight,
            slotWidth, slotHeight );
        target.adjust( 10, 10, -10, -10 ); // Borders
        double scale;
        if( target.width() / double( w->width() ) < target.height() / double( w->height() ))
            { // Center vertically
            scale = target.width() / double( w->width() );
            target.moveTop( target.top() + ( target.height() - int( w->height() * scale )) / 2 );
            target.setHeight( int( w->height() * scale ));
            }
        else
            { // Center horizontally
            scale = target.height() / double( w->height() );
            target.moveLeft( target.left() + ( target.width() - int( w->width() * scale )) / 2 );
            target.setWidth( int( w->width() * scale ));
            }
        // Don't scale the windows too much
        if( scale > 2.0 || ( scale > 1.0 && ( w->width() > 300 || w->height() > 300 )))
        {
            scale = ( w->width() > 300 || w->height() > 300 ) ? 1.0 : 2.0;
            target = QRect(
                target.center().x() - int( w->width() * scale ) / 2,
                target.center().y() - int( w->height() * scale ) / 2,
                scale * w->width(), scale * w->height() );
        }
        m_motionManager.moveWindow( w, target );
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsKompose( EffectWindowList windowlist, int screen )
    {
    QRect availRect = effects->clientArea( ScreenArea, screen, effects->currentDesktop() );
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
            double ratio = aspectRatio( window );

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
            if (j != columns-1 && it != windowlist.end() && aspectRatio(*it) < 1)
                {
                int addW = w - widthForHeight(*it, h);
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
                double widthByHeight = widthForHeight( window, usableH );
                double heightByWidth = heightForWidth( window, usableW );
                if ( (ratio >= 1.0 && heightByWidth <= usableH) ||
                      (ratio < 1.0 && widthByHeight > usableW)   )
                    {
                    widgetw = usableW;
                    widgeth = (int)heightByWidth;
                    }
                else if ( (ratio < 1.0 && widthByHeight <= usableW) ||
                           (ratio >= 1.0 && heightByWidth > usableH)   )
                    {
                    widgeth = usableH;
                    widgetw = (int)widthByHeight;
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
            QRect target = geometryRects[pos];
            target.setY( target.y() + topOffset );
            m_windowData[window].slot = pos;
            m_motionManager.moveWindow( window, target );

            //kDebug(1212) << "Window '" << window->caption() << "' gets moved to (" <<
            //        mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            //        "), scale: " << mWindowData[window].scale << endl;
            }
        if ( maxRowHeights[i]-h > 0 )
            topOffset += maxRowHeights[i]-h;
        }
    }

void PresentWindowsEffect::calculateWindowTransformationsNatural( EffectWindowList windowlist, int screen )
    {
    if( windowlist.count() == 1 )
        { // No idea why this is needed but if there is only one window on a Xinerama screen
          // then the window is scaled into nothingness at (0,0) if its position isn't reset.
        m_motionManager.reset( windowlist[0] );
        return;
        }

    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    qSort( windowlist );

    QRect area = effects->clientArea( ScreenArea, screen, effects->currentDesktop() );
    QRect bounds = area;
    int direction = 0;
    QHash<EffectWindow*, QRect> targets;
    foreach( EffectWindow *w, windowlist )
        {
        bounds = bounds.united( w->geometry() );
        targets[w] = w->geometry();
        // Reuse the unused "slot" as a preferred direction attribute. This is used when the window
        // is on the edge of the screen to try to use as much screen real estate as possible.
        m_windowData[w].slot = direction;
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
        foreach( EffectWindow *w, windowlist )
            {
            foreach( EffectWindow *e, windowlist )
                {
                if( w != e && targets[w].adjusted( -5, -5, 5, 5 ).intersects(
                              targets[e].adjusted( -5, -5, 5, 5 )))
                    {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff( targets[e].center() - targets[w].center() );
                    // Prevent dividing by zero and non-movement
                    if( diff.x() == 0 && diff.y() == 0 )
                        diff.setX( 1 );
                    // Try to keep screen aspect ratio
                    //if( bounds.height() / bounds.width() > area.height() / area.width() )
                    //    diff.setY( diff.y() / 2 );
                    //else
                    //    diff.setX( diff.x() / 2 );
                    // Approximate a vector of between 10px and 20px in magnitude in the same direction
                    diff *= m_accuracy / double( diff.manhattanLength() );
                    // Move both windows apart
                    targets[w].translate( -diff );
                    targets[e].translate( diff );

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
                    int xSection = ( targets[w].x() - bounds.x() ) / ( bounds.width() / 3 );
                    int ySection = ( targets[w].y() - bounds.y() ) / ( bounds.height() / 3 );
                    diff = QPoint( 0, 0 );
                    if( xSection != 1 || ySection != 1 ) // Remove this if you want the center to pull as well
                        {
                        if( xSection == 1 )
                            xSection = ( m_windowData[w].slot / 2 ? 2 : 0 );
                        if( ySection == 1 )
                            ySection = ( m_windowData[w].slot % 2 ? 2 : 0 );
                        }
                    if( xSection == 0 && ySection == 0 )
                        diff = QPoint( bounds.topLeft() - targets[w].center() );
                    if( xSection == 2 && ySection == 0 )
                        diff = QPoint( bounds.topRight() - targets[w].center() );
                    if( xSection == 2 && ySection == 2 )
                        diff = QPoint( bounds.bottomRight() - targets[w].center() );
                    if( xSection == 0 && ySection == 2 )
                        diff = QPoint( bounds.bottomLeft() - targets[w].center() );
                    if( diff.x() != 0 || diff.y() != 0 )
                        {
                        diff *= m_accuracy / double( diff.manhattanLength() );
                        targets[w].translate( diff );
                        }
    
                    // Update bounding rect
                    bounds = bounds.united( targets[w] );
                    bounds = bounds.united( targets[e] );
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
    foreach( EffectWindow *w, windowlist )
        targets[w] = QRect(
            ( targets[w].x() - bounds.x() ) * scale + area.x(),
            ( targets[w].y() - bounds.y() ) * scale + area.y(),
            targets[w].width() * scale,
            targets[w].height() * scale
            );

    // Try to fill the gaps by enlarging windows if they have the space
    if( m_fillGaps )
        {
        // Don't expand onto or over the border
        QRegion borderRegion( area.adjusted( -200, -200, 200, 200 ));
        borderRegion ^= area.adjusted( 10 / scale, 10 / scale, -10 / scale, -10 / scale );
        
        bool moved;
        do
            {
            moved = false;
            foreach( EffectWindow *w, windowlist )
                {
                QRect oldRect;
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = m_accuracy;
                int heightDiff = heightForWidth( w, targets[w].width() + widthDiff ) - targets[w].height();
                int xDiff = widthDiff / 2;  // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.
        
                // Attempt enlarging to the top-right
                oldRect = targets[w];
                targets[w] = QRect(
                    targets[w].x() + xDiff,
                    targets[w].y() - yDiff - heightDiff,
                    targets[w].width() + widthDiff,
                    targets[w].height() + heightDiff
                    );
                if( isOverlappingAny( w, targets, borderRegion ))
                    targets[w] = oldRect;
                else
                    moved = true;
                
                // Attempt enlarging to the bottom-right
                oldRect = targets[w];
                targets[w] = QRect(
                    targets[w].x() + xDiff,
                    targets[w].y() + yDiff,
                    targets[w].width() + widthDiff,
                    targets[w].height() + heightDiff
                    );
                if( isOverlappingAny( w, targets, borderRegion ))
                    targets[w] = oldRect;
                else
                    moved = true;
                
                // Attempt enlarging to the bottom-left
                oldRect = targets[w];
                targets[w] = QRect(
                    targets[w].x() - xDiff - widthDiff,
                    targets[w].y() + yDiff,
                    targets[w].width() + widthDiff,
                    targets[w].height() + heightDiff
                    );
                if( isOverlappingAny( w, targets, borderRegion ))
                    targets[w] = oldRect;
                else
                    moved = true;
                
                // Attempt enlarging to the top-left
                oldRect = targets[w];
                targets[w] = QRect(
                    targets[w].x() - xDiff - widthDiff,
                    targets[w].y() - yDiff - heightDiff,
                    targets[w].width() + widthDiff,
                    targets[w].height() + heightDiff
                    );
                if( isOverlappingAny( w, targets, borderRegion ))
                    targets[w] = oldRect;
                else
                    moved = true;
                }
            } while( moved );

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        foreach( EffectWindow *w, windowlist )
            {
            double scale = targets[w].width() / double( w->width() );
            if( scale > 2.0 || ( scale > 1.0 && ( w->width() > 300 || w->height() > 300 )))
                {
                    scale = ( w->width() > 300 || w->height() > 300 ) ? 1.0 : 2.0;
                    targets[w] = QRect(
                        targets[w].center().x() - int( w->width() * scale ) / 2,
                        targets[w].center().y() - int( w->height() * scale ) / 2,
                        w->width() * scale,
                        w->height() * scale );
                }
            }
        }

    // Notify the motion manager of the targets
    foreach( EffectWindow *w, windowlist )
        m_motionManager.moveWindow( w, targets[w] );
    }

//-----------------------------------------------------------------------------
// Helper functions for window rearranging

void PresentWindowsEffect::assignSlots( EffectWindowList windowlist, const QRect &area, int columns, int rows )
    {
    QVector< bool > taken;
    taken.fill( false, columns * rows );
    foreach( EffectWindow* w, windowlist )
        if( m_windowData[w].slot != -1 )
            taken[ m_windowData[w].slot ] = true;
    int slotWidth = area.width() / columns;
    int slotHeight = area.height() / rows;
    if( m_tabBoxEnabled )
        {
        for( int i = 0; i < windowlist.count(); i++ )
            {
            EffectWindow *w = windowlist[i];
            WindowData *wData = &m_windowData[w];
            if( wData->slot != -1 )
                continue; // It's already has a slot
            int x = i % columns;
            int y = i / columns;
            QPoint pos = w->geometry().center();
            if( pos.x() < area.left() )
                pos.setX( area.left() );
            if( pos.x() > area.right() )
                pos.setX( area.right() );
            if( pos.y() < area.top() )
                pos.setY( area.top() );
            if( pos.y() > area.bottom() )
                pos.setY( area.bottom() );
            int xdiff = pos.x() - ( area.x() + slotWidth * x + slotWidth / 2 );
            int ydiff = pos.y() - ( area.y() + slotHeight * y + slotHeight / 2 );
            int dist = int( sqrt( double( xdiff * xdiff + ydiff * ydiff )));
            wData->slot = i;
            wData->slot_distance = dist;
            }
        }
    else
        {
        foreach( EffectWindow *w, windowlist )
            {
            WindowData *wData = &m_windowData[w];
            if( wData->slot != -1 )
                continue; // it already has a slot
            QPoint pos = w->geometry().center();
            if( pos.x() < area.left() )
                pos.setX( area.left() );
            if( pos.x() > area.right() )
                pos.setX( area.right() );
            if( pos.y() < area.top() )
                pos.setY( area.top() );
            if( pos.y() > area.bottom() )
                pos.setY( area.bottom() );
            int distance = INT_MAX;
            for( int x = 0; x < columns; x++ )
                for( int y = 0; y < rows; y++ )
                    {
                    int slot = x + y * columns;
                    if( taken[slot] )
                        continue;
                    int xdiff = pos.x() - ( area.x() + slotWidth * x + slotWidth / 2 );
                    int ydiff = pos.y() - ( area.y() + slotHeight * y + slotHeight / 2 );
                    int dist = int( sqrt( double( xdiff * xdiff + ydiff * ydiff )));
                    if( dist < distance )
                        {
                        distance = dist;
                        wData->slot = slot;
                        wData->slot_distance = distance;
                        }
                    }
            }
        }
    }

void PresentWindowsEffect::getBestAssignments( EffectWindowList windowlist )
    {
    foreach( EffectWindow* w1, windowlist )
        {
        WindowData *windowData1 = &m_windowData[w1];
        foreach( EffectWindow* w2, windowlist )
            {
            WindowData *windowData2 = &m_windowData[w2];
            if( w1 != w2 && windowData1->slot == windowData2->slot &&
                windowData1->slot_distance >= windowData2->slot_distance )
                windowData1->slot = -1;
            }
        }
    }

bool PresentWindowsEffect::isOverlappingAny( EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border )
    {
    if( border.intersects( targets[w] ))
        return true;
    // Is there a better way to do this?
    foreach( EffectWindow *e, targets.keys() )
        {
        if( w == e )
            continue;
        if( targets[w].adjusted( -5, -5, 5, 5 ).intersects(
            targets[e].adjusted( -5, -5, 5, 5 )))
            return true;
        }
    return false;
    }

//-----------------------------------------------------------------------------
// Activation

void PresentWindowsEffect::setActive( bool active )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( m_activated == active )
        return;
    if( m_activated && m_tabBoxEnabled )
        {
        effects->closeTabBox();
        return;
        }
    m_activated = active;
    if( m_activated )
        {
        m_decalOpacity = 0.0;
        m_windowFilter.clear();

        // Add every single window to m_windowData (Just calling [w] creates it)
        foreach( EffectWindow *w, effects->stackingOrder() )
            {
            m_windowData[w].visible = isVisibleWindow( w );
            m_windowData[w].opacity = w->isOnCurrentDesktop() ? 1.0 : 0.0;
            if( !m_tabBoxEnabled && m_ignoreMinimized && w->isMinimized() )
                m_windowData[w].opacity = 0.0;
            m_windowData[w].highlight = 1.0;
            }

        if( m_tabBoxEnabled )
            {
            foreach( EffectWindow *w, effects->currentTabBoxWindowList() )
                {
                m_motionManager.manage( w );
                m_windowData[w].visible = effects->currentTabBoxWindowList().contains( w );
                }
            // Hide windows not in the list
            foreach( EffectWindow *w, effects->stackingOrder() )
                m_windowData[w].visible = isVisibleWindow( w ) &&
                    (!isSelectableWindow( w ) || effects->currentTabBoxWindowList().contains( w ));
            }
        else
            {
            // Filter out special windows such as panels and taskbars
            foreach( EffectWindow *w, effects->stackingOrder() )
                if( isSelectableWindow( w ))
                    m_motionManager.manage( w );
            }
        if( m_motionManager.managedWindows().count() < 2 )
            { // No point triggering if there is nothing to do
            m_activated = false;
            m_windowData.clear();
            m_motionManager.unmanageAll();
            return;
            }

        // Create temporary input window to catch mouse events
        m_input = effects->createFullScreenInputWindow( this, Qt::PointingHandCursor );
        m_hasKeyboardGrab = effects->grabKeyboard( this );
        effects->setActiveFullScreenEffect( this );

        m_gridSizes.clear();
        for( int i = 0; i < effects->numScreens(); i++ )
            m_gridSizes.append( GridSize() );

        rearrangeWindows();
        if( m_tabBoxEnabled )
            setHighlightedWindow( effects->currentTabBoxWindow() );
        else
            setHighlightedWindow( effects->activeWindow() );
        }
    else
        {
        // Fade in/out all windows
        foreach( EffectWindow *w, effects->stackingOrder() )
            {
            // TODO: w->isOnCurrentDesktop(); doesn't seem to be updated immediately
            if( m_highlightedWindow )
                m_windowData[w].visible = ( w->desktop() == m_highlightedWindow->desktop() || w->isOnAllDesktops() ) && !w->isMinimized();
            else // Only called when cancelling the effect, so isOnCurrentDesktop() is fine
                m_windowData[w].visible = w->isOnCurrentDesktop() && !w->isMinimized();
            }

        // Move all windows back to their original position
        foreach( EffectWindow *w, m_motionManager.managedWindows() )
            m_motionManager.moveWindow( w, w->geometry() );
        discardFilterTexture();
        m_windowFilter.clear();

        effects->destroyInputWindow( m_input );
        if( m_hasKeyboardGrab )
            effects->ungrabKeyboard();
        m_hasKeyboardGrab = false;
        }
    effects->addRepaintFull(); // Trigger the first repaint
    }

//-----------------------------------------------------------------------------
// Filter box

void PresentWindowsEffect::discardFilterTexture()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    delete m_filterTexture;
    m_filterTexture = NULL;
#endif
    }

void PresentWindowsEffect::updateFilterTexture()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    discardFilterTexture();
    if( m_windowFilter.isEmpty())
        {
        effects->addRepaint( m_filterTextureRect );
        return;
        }
    // Create font for filter text
    QFont font;
    font.setPointSize( font.pointSize() * 2 );
    font.setBold( true );
    // Get size of the rect containing filter text
    QFontMetrics fm( font );
    QRect rect;
    QString translatedString = i18n( "Filter:\n%1", m_windowFilter );
    rect.setSize( fm.size( 0, translatedString ));
    QRect area = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop() );
    // Create image
    QImage im( rect.width(), rect.height(), QImage::Format_ARGB32 );
    im.fill( Qt::transparent );
    // Paint the filter text to it
    QPainter p( &im );
    p.setFont( font );
    p.setPen( QPalette().color( QPalette::Active, QPalette::HighlightedText ));
    p.drawText( rect, Qt::AlignCenter, translatedString );
    p.end();
    // Create GL texture
    m_filterTexture = new GLTexture( im );
    // Get position for filter text and it's frame
    m_filterTextureRect = QRect(
        area.x() + ( area.width() - rect.width() ) / 2,
        area.y() + ( area.height() - rect.height() ) / 2,
        rect.width(), rect.height() );
    m_filterFrameRect = m_filterTextureRect.adjusted( -20, -10, 20, 10 );
    // Schedule repaint
    effects->addRepaint( m_filterTextureRect );
#endif
    }

//-----------------------------------------------------------------------------
// Helper functions

bool PresentWindowsEffect::isSelectableWindow( EffectWindow *w )
    {
    if( w->isSpecialWindow() || w->isUtility() )
        return false;
    if( w->isDeleted() )
        return false;
    if( !m_allDesktops && !w->isOnCurrentDesktop() )
        return false;
    if( !m_tabBoxEnabled && m_ignoreMinimized && w->isMinimized() )
        return false;
    return true;
    }

bool PresentWindowsEffect::isVisibleWindow( EffectWindow *w )
    {
    if( w->isDesktop() )
        return true;
    return isSelectableWindow( w );
    }

void PresentWindowsEffect::setHighlightedWindow( EffectWindow *w )
    {
    if( w == m_highlightedWindow || ( w != NULL && !m_motionManager.isManaging( w )))
        return;

    if( m_highlightedWindow )
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint
    m_highlightedWindow = w;
    if( m_highlightedWindow )
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint

    if( m_tabBoxEnabled && m_highlightedWindow )
        effects->setTabBoxWindow( w );
    }

EffectWindow* PresentWindowsEffect::relativeWindow( EffectWindow *w, int xdiff, int ydiff, bool wrap ) const
    { // TODO: Is it possible to select hidden windows?
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
                QRectF wArea = m_motionManager.transformedGeometry( w );
                detectRect = QRect( 0, wArea.y(), area.width(), wArea.height() );
                next = NULL;
                foreach( EffectWindow* e, m_motionManager.managedWindows() )
                    {
                    QRectF eArea = m_motionManager.transformedGeometry( e );
                    if( eArea.intersects( detectRect ) &&
                        eArea.x() > wArea.x() )
                        {
                        if( next == NULL )
                            next = e;
                        else
                            {
                            QRectF nArea = m_motionManager.transformedGeometry( next );
                            if( eArea.x() < nArea.x() )
                                next = e;
                            }
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
                QRectF wArea = m_motionManager.transformedGeometry( w );
                detectRect = QRect( 0, wArea.y(), area.width(), wArea.height() );
                next = NULL;
                foreach( EffectWindow* e, m_motionManager.managedWindows() )
                    {
                    QRectF eArea = m_motionManager.transformedGeometry( e );
                    if( eArea.intersects( detectRect ) &&
                        eArea.x() + eArea.width() < wArea.x() + wArea.width() )
                        {
                        if( next == NULL )
                            next = e;
                        else
                            {
                            QRectF nArea = m_motionManager.transformedGeometry( next );
                            if( eArea.x() + eArea.width() > nArea.x() + nArea.width() )
                                next = e;
                            }
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
                QRectF wArea = m_motionManager.transformedGeometry( w );
                detectRect = QRect( wArea.x(), 0, wArea.width(), area.height() );
                next = NULL;
                foreach( EffectWindow* e, m_motionManager.managedWindows() )
                    {
                    QRectF eArea = m_motionManager.transformedGeometry( e );
                    if( eArea.intersects( detectRect ) &&
                        eArea.y() > wArea.y() )
                        {
                        if( next == NULL )
                            next = e;
                        else
                            {
                            QRectF nArea = m_motionManager.transformedGeometry( next );
                            if( eArea.y() < nArea.y() )
                                next = e;
                            }
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
                QRectF wArea = m_motionManager.transformedGeometry( w );
                detectRect = QRect( wArea.x(), 0, wArea.width(), area.height() );
                next = NULL;
                foreach( EffectWindow* e, m_motionManager.managedWindows() )
                    {
                    QRectF eArea = m_motionManager.transformedGeometry( e );
                    if( eArea.intersects( detectRect ) &&
                        eArea.y() + eArea.height() < wArea.y() + wArea.height() )
                        {
                        if( next == NULL )
                            next = e;
                        else
                            {
                            QRectF nArea = m_motionManager.transformedGeometry( next );
                            if( eArea.y() + eArea.height() > nArea.y() + nArea.height() )
                                next = e;
                            }
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

    abort(); // Should never get here
    }

EffectWindow* PresentWindowsEffect::findFirstWindow() const
    {
    EffectWindow *topLeft = NULL;
    QRectF topLeftGeometry;
    foreach( EffectWindow *w, m_motionManager.managedWindows() )
        {
        QRectF geometry = m_motionManager.transformedGeometry( w );
        if( m_windowData[w].visible == false )
            continue; // Not visible
        if( topLeft == NULL )
            {
            topLeft = w;
            topLeftGeometry = geometry;
            }
        else if( geometry.x() < topLeftGeometry.x() || geometry.y() < topLeftGeometry.y() )
            {
            topLeft = w;
            topLeftGeometry = geometry;
            }
        }
    return topLeft;
    }

void PresentWindowsEffect::paintWindowIcon( EffectWindow *w, WindowPaintData &data )
    {
    // Don't bother if we don't even have an icon
    if( w->icon().isNull() )
        return;

    WindowData &wData = m_windowData[w];
    if( wData.icon.cacheKey() != w->icon().cacheKey())
        { // Make sure data.icon is the right QPixmap, and rebind
        wData.icon = w->icon();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing )
            {
            wData.iconTexture = new GLTexture( wData.icon );
            wData.iconTexture->setFilter( GL_LINEAR );
            }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing )
            wData.iconPicture = XRenderPicture( wData.icon );
#endif
        }
    int icon_margin = 8;
    int width = wData.icon.width();
    int height = wData.icon.height();
    int x = w->x() + data.xTranslate + w->width() * data.xScale * 0.95 - width - icon_margin;
    int y = w->y() + data.yTranslate + w->height() * data.yScale * 0.95 - height - icon_margin;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        // Render some background
        glColor4f( 0, 0, 0, 0.5 * wData.opacity * m_decalOpacity );
        renderRoundBox( QRect( x-3, y-3, width+6, height+6 ), 3 );
        // Render the icon
        glColor4f( 1, 1, 1, 1 * wData.opacity * m_decalOpacity );
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        wData.iconTexture->bind();
        wData.iconTexture->render( infiniteRegion(), QRect( x, y, width, height ));
        wData.iconTexture->unbind();
        glPopAttrib();
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        XRenderComposite( display(),
            wData.icon.depth() == 32 ? PictOpOver : PictOpSrc,
            wData.iconPicture, None,
            effects->xrenderBufferPicture(),
            0, 0, 0, 0, x, y, width, height );
        }
#endif
    }

} // namespace

#include "presentwindows.moc"
