/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "presentwindows.h"

#include <client.h>
#include <workspace.h>

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>

#include <math.h>



// Note that currently effects need to be manually enabled in the EffectsHandler
// class constructor (in effects.cpp).

namespace KWinInternal
{


PresentWindowsEffect::PresentWindowsEffect(Workspace* ws) : Effect()
    {
    mWorkspace = ws;
    mActivated = false;
    mActiveness = 0.0;

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Expose effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
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

    // We need to mark the screen as transformed. Otherwise the whole screen
    //  won't be repainted, resulting in artefacts
    if( mActiveness > 0.0f )
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED;

    effects->prePaintScreen(mask, region, time);
    }

void PresentWindowsEffect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    if( mActiveness > 0.0f && mWindowData.contains(w->window()) )
        // This window will be transformed by the effect
        *mask |= Scene::PAINT_WINDOW_TRANSFORMED;

    effects->prePaintWindow( w, mask, region, time );
    }

void PresentWindowsEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if(mActiveness > 0.0f && mWindowData.contains(w->window()))
        {
        // Change window's position and scale
        const WindowData& windata = mWindowData[w->window()];
        data.xScale = interpolate(data.xScale, windata.scale, mActiveness);
        data.yScale = interpolate(data.xScale, windata.scale, mActiveness);
        data.xTranslate = (int)interpolate(data.xTranslate, windata.area.left() - w->window()->x(), mActiveness);
        data.yTranslate = (int)interpolate(data.yTranslate, windata.area.top() - w->window()->y(), mActiveness);
        // Darken all windows except for the one under the cursor
        if( !windata.area.contains(cursorPos()) )
            data.brightness *= interpolate(1.0, 0.7, mActiveness);
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void PresentWindowsEffect::postPaintScreen()
    {
    // If mActiveness is between 0 and 1, the effect is still in progress and the
    //  workspace has to be repainted during the next pass
    if( mActiveness > 0.0 && mActiveness < 1.0 )
        mWorkspace->addDamageFull(); // trigger next animation repaint

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
    for( QMap<Toplevel*, WindowData>::iterator it = mWindowData.begin();
         it != mWindowData.end(); ++it )
        {
        if( it.value().area.contains(pos) )
            {
            effects->activateWindow( (Client*)it.key() );  // All keys of mWindowData are Clients anyway
            // Prevent re-minimizing when the effect terminates
            mTemporarilyUnminimized.removeAll( (Client*)it.key() );
            }
        }

    // Deactivate effect, no matter if any window was actually activated
    setActive(false);
    }

void PresentWindowsEffect::windowActivated( Toplevel* t )
    {
        rearrangeWindows();
    }

void PresentWindowsEffect::windowClosed( Toplevel* t, Deleted* )
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
    mInput = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(), Qt::PointingHandCursor );
    // TODO: maybe also create a KAction so that ressing Esc would terminate the effect?
    }

void PresentWindowsEffect::effectTerminated()
    {
    // We need to re-minimize temporarily minimized windows
    foreach( Client* client, mTemporarilyUnminimized )
        client->minimize(true);

    // Destroy the temporary input window
    effects->destroyInputWindow( mInput );
    }

void PresentWindowsEffect::rearrangeWindows()
    {
    if( !mActivated )
        return;

    mTemporarilyUnminimized.clear();
    mWindowData.clear();

    const ClientList& originalclientlist = mWorkspace->stackingOrder();
    // Filter out special windows such as panels and taskbars
    ClientList clientlist;
    foreach( Client* client, originalclientlist )
        {
        if( client->isSpecialWindow() )
            continue;
        // TODO: make it possible to show windows of all desktops (needs config
        //  option)
        if( !client->isOnCurrentDesktop() )
            continue;
        // Temporarily restore minimized windows
        // TODO: make this configurable
        if( client->isMinimized() )
            {
            mTemporarilyUnminimized.append(client);
            client->unminimize(true);  // true = avoid animation
            }
        clientlist.append(client);
        }

    // Calculate new positions and scales for windows
    calculateWindowTransformationsKompose( clientlist );

    // Schedule entire desktop to be repainted
    mWorkspace->addDamageFull();
    }

void PresentWindowsEffect::calculateWindowTransformationsDumb(ClientList clientlist)
    {
    // Calculate number of rows/cols
    int rows = clientlist.count() / 4 + 1;
    int cols = clientlist.count() / rows + clientlist.count() % rows;
    // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect placementRect = mWorkspace->clientArea( PlacementArea, QPoint( 0, 0 ), 0 );
    // Size of one cell
    int cellwidth = placementRect.width() / cols;
    int cellheight = placementRect.height() / rows;
    kDebug() << k_funcinfo << "Got " << clientlist.count() << " clients, using " << rows << "x" << cols << " grid" << endl;

    // Calculate position and scale factor for each window
    int i = 0;
    foreach( Client* client, clientlist )
        {

        // Row/Col of this window
        int r = i / cols;
        int c = i % cols;
        mWindowData[client].scale = qMin(cellwidth / (float)client->width(), cellheight / (float)client->height());
        mWindowData[client].area.setLeft(placementRect.left() + cellwidth * c);
        mWindowData[client].area.setTop(placementRect.top() + cellheight * r);
        mWindowData[client].area.setWidth((int)(client->width() * mWindowData[client].scale));
        mWindowData[client].area.setHeight((int)(client->height() * mWindowData[client].scale));

        kDebug() << k_funcinfo << "Window '" << client->caption() << "' gets moved to (" <<
            mWindowData[client].area.left() << "; " << mWindowData[client].area.right() <<
            "), scale: " << mWindowData[client].scale << endl;
        i++;
        }
    }

float PresentWindowsEffect::clientAspectRatio(Client* c)
    {
    return c->width() / (float)c->height();
    }

int PresentWindowsEffect::clientWidthForHeight(Client* c, int h)
    {
    return (int)((h / (float)c->height()) * c->width());
    }

int PresentWindowsEffect::clientHeightForWidth(Client* c, int w)
    {
    return (int)((w / (float)c->width()) * c->height());
    }

void PresentWindowsEffect::calculateWindowTransformationsKompose(ClientList clientlist)
    {
     // Get rect which we can use on current desktop. This excludes e.g. panels
    QRect availRect = mWorkspace->clientArea( PlacementArea, QPoint( 0, 0 ), 0 );

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp

    int spacing = 10;
    int rows, columns;
    float parentRatio = availRect.width() / (float)availRect.height();
    // Use more columns than rows when parent's width > parent's height
    if ( parentRatio > 1 )
    {
        columns = (int)ceil( sqrt(clientlist.count()) );
        rows = (int)ceil( (double)clientlist.count() / (double)columns );
    }
    else
    {
        rows = (int)ceil( sqrt(clientlist.count()) );
        columns = (int)ceil( (double)clientlist.count() / (double)rows );
    }
    kDebug() << k_funcinfo << "Using " << rows << " rows & " << columns << " columns for " << clientlist.count() << " clients" << endl;

    // Calculate width & height
    int w = (availRect.width() - (columns+1) * spacing ) / columns;
    int h = (availRect.height() - (rows+1) * spacing ) / rows;

    ClientList::iterator it( clientlist.begin() );
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
            Client *client;

            // Check for end of List
            if ( it == clientlist.end() )
                break;
            client = *it;

            // Calculate width and height of widget
            float ratio = clientAspectRatio(client);

            int widgetw = 100;
            int widgeth = 100;
            int usableW = w;
            int usableH = h;

            // use width of two boxes if there is no right neighbour
            if (client == clientlist.last() && j != columns-1)
                {
                usableW = 2*w;
                }
            ++it; // We need access to the neighbour in the following
            // expand if right neighbour has ratio < 1
            if (j != columns-1 && it != clientlist.end() && clientAspectRatio(*it) < 1)
                {
                int addW = w - clientWidthForHeight(*it, h);
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
                double widthForHeight = clientWidthForHeight(client, usableH);
                double heightForWidth = clientHeightForWidth(client, usableW);
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
            if(pos >= clientlist.count())
                break;

            Client* client = clientlist[pos];
            QRect geom = geometryRects[pos];
            geom.setY( geom.y() + topOffset );
            mWindowData[client].area = geom;
            mWindowData[client].scale = geom.width() / (float)client->width();

            kDebug() << k_funcinfo << "Window '" << client->caption() << "' gets moved to (" <<
                    mWindowData[client].area.left() << "; " << mWindowData[client].area.right() <<
                    "), scale: " << mWindowData[client].scale << endl;
            }
        if ( maxRowHeights[i]-h > 0 )
            topOffset += maxRowHeights[i]-h;
        }
    }


} // namespace
#include "presentwindows.moc"

