/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This file contains things relevant to direct user actions, such as
 responses to global keyboard shortcuts, or selecting actions
 from the window operations menu.

*/

#include "client.h"
#include "workspace.h"

#include <fixx11h.h>
#include <qhbox.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qtooltip.h>
#include <qpopupmenu.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobalaccel.h>
#include <kapplication.h>
#include <qregexp.h>

#include "popupinfo.h"
#include "killwindow.h"
#include "tabbox.h"

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

QPopupMenu* Workspace::clientPopup()
    {
    if ( !popup )
        {
        popup = new QPopupMenu;
        popup->setCheckable( TRUE );
        popup->setFont(KGlobalSettings::menuFont());
        connect( popup, SIGNAL( aboutToShow() ), this, SLOT( clientPopupAboutToShow() ) );
        connect( popup, SIGNAL( activated(int) ), this, SLOT( clientPopupActivated(int) ) );
      
        advanced_popup = new QPopupMenu( popup );
        advanced_popup->setCheckable( TRUE );
        advanced_popup->setFont(KGlobalSettings::menuFont());
        connect( advanced_popup, SIGNAL( activated(int) ), this, SLOT( clientPopupActivated(int) ) );
        advanced_popup->insertItem( SmallIconSet( "up" ),
            i18n("Keep &Above Others")+'\t'+keys->shortcut("Window Above Other Windows").seq(0).toString(), Options::KeepAboveOp );
        advanced_popup->insertItem( SmallIconSet( "down" ),
            i18n("Keep &Below Others")+'\t'+keys->shortcut("Window Below Other Windows").seq(0).toString(), Options::KeepBelowOp );
        advanced_popup->insertItem( SmallIconSet( "window_fullscreen" ),
            i18n("&Fullscreen")+'\t'+keys->shortcut("Window Fullscreen").seq(0).toString(), Options::FullScreenOp );
        advanced_popup->insertItem( i18n("&No Border")+'\t'+keys->shortcut("Window No Border").seq(0).toString(), Options::NoBorderOp );
        advanced_popup->insertItem( SmallIconSet("key_bindings"),
            i18n("Window &Shortcut...")+'\t'+keys->shortcut("Setup Window Shortcut").seq(0).toString(), Options::SetupWindowShortcutOp );
        advanced_popup->insertItem( SmallIconSet( "wizard" ), i18n("&Special Window Settings..."), Options::WindowRulesOp );

        popup->insertItem(i18n("Ad&vanced"), advanced_popup );
        desk_popup_index = popup->count();
        
        if (options->useTranslucency){
            QPopupMenu *trans_popup = new QPopupMenu( popup );
            QVBox *transBox = new QVBox(trans_popup);
            transButton = new QPushButton(transBox, "transButton");
            QToolTip::add(transButton, i18n("Reset opacity to default value"));
            transSlider = new QSlider(0, 100, 1, 100, Qt::Vertical, transBox, "transSlider");
            QToolTip::add(transSlider, i18n("Slide this to set the window's opacity"));
            connect(transButton, SIGNAL(clicked()), SLOT(resetClientOpacity()));
            connect(transButton, SIGNAL(clicked()), trans_popup, SLOT(hide()));
            connect(transSlider, SIGNAL(valueChanged(int)), SLOT(setTransButtonText(int)));
            connect(transSlider, SIGNAL(sliderMoved(int)), this, SLOT(setPopupClientOpacity(int)));
            connect(transSlider, SIGNAL(sliderReleased()), trans_popup, SLOT(hide()));
            trans_popup->insertItem(transBox);
            popup->insertItem(i18n("&Opacity"), trans_popup );
        }
        
        popup->insertItem( SmallIconSet( "move" ), i18n("&Move")+'\t'+keys->shortcut("Window Move").seq(0).toString(), Options::MoveOp );
        popup->insertItem( i18n("Re&size")+'\t'+keys->shortcut("Window Resize").seq(0).toString(), Options::ResizeOp );
        popup->insertItem( i18n("Mi&nimize")+'\t'+keys->shortcut("Window Minimize").seq(0).toString(), Options::MinimizeOp );
        popup->insertItem( i18n("Ma&ximize")+'\t'+keys->shortcut("Window Maximize").seq(0).toString(), Options::MaximizeOp );
        popup->insertItem( i18n("Sh&ade")+'\t'+keys->shortcut("Window Shade").seq(0).toString(), Options::ShadeOp );

        popup->insertSeparator();

        if (!KGlobal::config()->isImmutable() && 
            !kapp->authorizeControlModules(Workspace::configModules(true)).isEmpty())
            {
            popup->insertItem(SmallIconSet( "configure" ), i18n("Configur&e Window Behavior..."), this, SLOT( configureWM() ));
            popup->insertSeparator();
            }

        popup->insertItem( SmallIconSet( "fileclose" ), i18n("&Close")+'\t'+keys->shortcut("Window Close").seq(0).toString(), Options::CloseOp );
        }
    return popup;
    }
    
//sets the transparency of the client to given value(given by slider)
void Workspace::setPopupClientOpacity(int value)
    {
    active_popup_client->setCustomOpacityFlag(true);
    value = 100 - value;
    value<100?active_popup_client->setOpacity(true, (uint)((value/100.0)*0xffffffff)):active_popup_client->setOpacity(false,0xffffffff);
    }

void Workspace::setTransButtonText(int value)
    {
    value = 100 - value;
    if(value < 0)
        transButton->setText("000 %");
    else if (value >= 100 )
        transButton->setText("100 %");
    else if(value < 10)
        transButton->setText("00"+QString::number(value)+" %");
    else if(value < 100)
        transButton->setText("0"+QString::number(value)+" %");
    }

void Workspace::resetClientOpacity()
    {
    active_popup_client->setCustomOpacityFlag(false);
    active_popup_client->updateOpacity();
    transSlider->setValue(100-active_popup_client->opacityPercentage());
    setTransButtonText(100-active_popup_client->opacityPercentage());
    }


/*!
  The client popup menu will become visible soon.

  Adjust the items according to the respective popup client.
 */
void Workspace::clientPopupAboutToShow()
    {
    if ( !active_popup_client || !popup )
        return;

    if ( numberOfDesktops() == 1 )
        {
        delete desk_popup;
        desk_popup = 0;
        }
    else
        {
        initDesktopPopup();
        }

    popup->setItemEnabled( Options::ResizeOp, active_popup_client->isResizable() );
    popup->setItemEnabled( Options::MoveOp, active_popup_client->isMovable() );
    popup->setItemEnabled( Options::MaximizeOp, active_popup_client->isMaximizable() );
    popup->setItemChecked( Options::MaximizeOp, active_popup_client->maximizeMode() == Client::MaximizeFull );
    // This should be checked also when hover unshaded
    popup->setItemChecked( Options::ShadeOp, active_popup_client->shadeMode() != ShadeNone );
    popup->setItemEnabled( Options::ShadeOp, active_popup_client->isShadeable());
    advanced_popup->setItemChecked( Options::KeepAboveOp, active_popup_client->keepAbove() );
    advanced_popup->setItemChecked( Options::KeepBelowOp, active_popup_client->keepBelow() );
    advanced_popup->setItemChecked( Options::FullScreenOp, active_popup_client->isFullScreen() );
    advanced_popup->setItemEnabled( Options::FullScreenOp, active_popup_client->userCanSetFullScreen() );
    advanced_popup->setItemChecked( Options::NoBorderOp, active_popup_client->noBorder() );
    advanced_popup->setItemEnabled( Options::NoBorderOp, active_popup_client->userCanSetNoBorder() );
    popup->setItemEnabled( Options::MinimizeOp, active_popup_client->isMinimizable() );
    popup->setItemEnabled( Options::CloseOp, active_popup_client->isCloseable() );
    if (options->useTranslucency)
        {
        transSlider->setValue(100-active_popup_client->opacityPercentage());
        setTransButtonText(100-active_popup_client->opacityPercentage());
        }
    }


void Workspace::initDesktopPopup()
    {
    if (desk_popup)
        return;

    desk_popup = new QPopupMenu( popup );
    desk_popup->setCheckable( TRUE );
    desk_popup->setFont(KGlobalSettings::menuFont());
    connect( desk_popup, SIGNAL( activated(int) ),
             this, SLOT( slotSendToDesktop(int) ) );
    connect( desk_popup, SIGNAL( aboutToShow() ),
             this, SLOT( desktopPopupAboutToShow() ) );

    popup->insertItem(i18n("To &Desktop"), desk_popup, -1, desk_popup_index );
    }

/*!
  Adjusts the desktop popup to the current values and the location of
  the popup client.
 */
void Workspace::desktopPopupAboutToShow()
    {
    if ( !desk_popup )
        return;

    desk_popup->clear();
    desk_popup->insertItem( i18n("&All Desktops"), 0 );
    if ( active_popup_client && active_popup_client->isOnAllDesktops() )
        desk_popup->setItemChecked( 0, TRUE );
    desk_popup->insertSeparator( -1 );
    int id;
    const int BASE = 10;
    for ( int i = 1; i <= numberOfDesktops(); i++ ) 
        {
        QString basic_name("%1  %2");
        if (i<BASE)
            {
            basic_name.prepend('&');
            }
        id = desk_popup->insertItem(
                basic_name
                    .arg(i)
                    .arg( desktopName(i).replace( '&', "&&" )),
                i );
        if ( active_popup_client &&
             !active_popup_client->isOnAllDesktops() && active_popup_client->desktop()  == i )
            desk_popup->setItemChecked( id, TRUE );
        }
    }

void Workspace::closeActivePopup()
    {
    if( active_popup )
        {
        active_popup->close();
        active_popup = NULL;
        active_popup_client = NULL;
        }
    }

/*!
  Create the global accel object \c keys.
 */
void Workspace::initShortcuts()
    {
    keys = new KGlobalAccel( this );
#include "kwinbindings.cpp"
    readShortcuts();
    }

void Workspace::readShortcuts()
    {
    keys->readSettings();

    cutWalkThroughDesktops = keys->shortcut("Walk Through Desktops");
    cutWalkThroughDesktopsReverse = keys->shortcut("Walk Through Desktops (Reverse)");
    cutWalkThroughDesktopList = keys->shortcut("Walk Through Desktop List");
    cutWalkThroughDesktopListReverse = keys->shortcut("Walk Through Desktop List (Reverse)");
    cutWalkThroughWindows = keys->shortcut("Walk Through Windows");
    cutWalkThroughWindowsReverse = keys->shortcut("Walk Through Windows (Reverse)");

    keys->updateConnections();
    
    delete popup;
    popup = NULL; // so that it's recreated next time
    desk_popup = NULL;
    }


void Workspace::setupWindowShortcut( Client* c )
    {
    assert( client_keys_dialog == NULL );
    keys->setEnabled( false );
    client_keys->setEnabled( false );
    client_keys_dialog = new ShortcutDialog( c->shortcut());
    client_keys_client = c;
    connect( client_keys_dialog, SIGNAL( dialogDone( bool )), SLOT( setupWindowShortcutDone( bool )));
    QRect r = clientArea( ScreenArea, c );
    QSize size = client_keys_dialog->sizeHint();
    QPoint pos = c->pos() + c->clientPos();
    if( pos.x() + size.width() >= r.right())
        pos.setX( r.right() - size.width());
    if( pos.y() + size.height() >= r.bottom())
        pos.setY( r.bottom() - size.height());
    client_keys_dialog->move( pos );
    client_keys_dialog->show();
    active_popup = client_keys_dialog;
    active_popup_client = c;
    }

void Workspace::setupWindowShortcutDone( bool ok )
    {
    keys->setEnabled( true );
    client_keys->setEnabled( true );
    if( ok )
        {
        client_keys_client->setShortcut( KShortcut( client_keys_dialog->shortcut()).toString());
        }
    closeActivePopup();
    delete client_keys_dialog;
    client_keys_dialog = NULL;
    client_keys_client = NULL;
    }

void Workspace::clientShortcutUpdated( Client* c )
    {
    QString key = QString::number( c->window());
    client_keys->remove( key );
    if( !c->shortcut().isNull())
        {
        client_keys->insert( key, key );
        client_keys->setShortcut( key, c->shortcut());
        client_keys->setSlot( key, c, SLOT( shortcutActivated()));
        client_keys->setActionEnabled( key, true );
        }
    client_keys->updateConnections();
    }

void Workspace::clientPopupActivated( int id )
    {
    WindowOperation op = static_cast< WindowOperation >( id );
    Client* c = active_popup_client ? active_popup_client : active_client;
    QString type;
    switch( op )
        {
        case FullScreenOp:
            if( !c->isFullScreen() && c->userCanSetFullScreen())
                type = "fullscreenaltf3";
          break;
        case NoBorderOp:
            if( !c->noBorder() && c->userCanSetNoBorder())
                type = "noborderaltf3";
          break;
        default:
            break;
        };
    if( !type.isEmpty())
        helperDialog( type, c );
    performWindowOperation( c, op );
    }


void Workspace::performWindowOperation( Client* c, Options::WindowOperation op ) 
    {
    if ( !c )
        return;

    if (op == Options::MoveOp || op == Options::UnrestrictedMoveOp )
        QCursor::setPos( c->geometry().center() );
    if (op == Options::ResizeOp || op == Options::UnrestrictedResizeOp )
        QCursor::setPos( c->geometry().bottomRight());
    switch ( op ) 
        {
        case Options::MoveOp:
            c->performMouseCommand( Options::MouseMove, QCursor::pos() );
            break;
        case Options::UnrestrictedMoveOp:
            c->performMouseCommand( Options::MouseUnrestrictedMove, QCursor::pos() );
            break;
        case Options::ResizeOp:
            c->performMouseCommand( Options::MouseResize, QCursor::pos() );
            break;
        case Options::UnrestrictedResizeOp:
            c->performMouseCommand( Options::MouseUnrestrictedResize, QCursor::pos() );
            break;
        case Options::CloseOp:
            c->closeWindow();
            break;
        case Options::MaximizeOp:
            c->maximize( c->maximizeMode() == Client::MaximizeFull
                ? Client::MaximizeRestore : Client::MaximizeFull );
            break;
        case Options::HMaximizeOp:
            c->maximize( c->maximizeMode() ^ Client::MaximizeHorizontal );
            break;
        case Options::VMaximizeOp:
            c->maximize( c->maximizeMode() ^ Client::MaximizeVertical );
            break;
        case Options::MinimizeOp:
            c->minimize();
            break;
        case Options::ShadeOp:
            c->performMouseCommand( Options::MouseShade, QCursor::pos());
            break;
        case Options::OnAllDesktopsOp:
            c->setOnAllDesktops( !c->isOnAllDesktops() );
            break;
        case Options::FullScreenOp:
            c->setFullScreen( !c->isFullScreen(), true );
            break;
        case Options::NoBorderOp:
            c->setUserNoBorder( !c->isUserNoBorder());
            break;
        case Options::KeepAboveOp:
            c->setKeepAbove( !c->keepAbove() );
            break;
        case Options::KeepBelowOp:
            c->setKeepBelow( !c->keepBelow() );
            break;
        case Options::WindowRulesOp:
            editWindowRules( c );
            break;
        case Options::SetupWindowShortcutOp:
            setupWindowShortcut( c );
            break;
        case Options::LowerOp:
            lowerClient(c);
            break;
        default:
            break;
        }
    }

/*!
  Performs a mouse command on this client (see options.h)
 */
bool Client::performMouseCommand( Options::MouseCommand command, QPoint globalPos, bool handled )
    {
    bool replay = FALSE;
    switch (command) 
        {
        case Options::MouseRaise:
            workspace()->raiseClient( this );
            break;
        case Options::MouseLower:
            workspace()->lowerClient( this );
            break;
        case Options::MouseShade :
            delete shadeHoverTimer;
            shadeHoverTimer = 0;
            toggleShade();
            break;
        case Options::MouseOperationsMenu:
            if ( isActive() & options->clickRaise )
                autoRaise();
            workspace()->showWindowMenu( globalPos, this );
            break;
        case Options::MouseToggleRaiseAndLower:
            workspace()->raiseOrLowerClient( this );
            break;
        case Options::MouseActivateAndRaise:
            replay = isActive(); // for clickraise mode
            workspace()->takeActivity( this, ActivityFocus | ActivityRaise, handled && replay );
            break;
        case Options::MouseActivateAndLower:
            workspace()->requestFocus( this );
            workspace()->lowerClient( this );
            break;
        case Options::MouseActivate:
            replay = isActive(); // for clickraise mode
            workspace()->takeActivity( this, ActivityFocus, handled && replay );
            break;
        case Options::MouseActivateRaiseAndPassClick:
            workspace()->takeActivity( this, ActivityFocus | ActivityRaise, handled );
            replay = TRUE;
            break;
        case Options::MouseActivateAndPassClick:
            workspace()->takeActivity( this, ActivityFocus, handled );
            replay = TRUE;
            break;
        case Options::MouseActivateRaiseAndMove:
        case Options::MouseActivateRaiseAndUnrestrictedMove:
            workspace()->raiseClient( this );
            workspace()->requestFocus( this );
            if( options->moveMode == Options::Transparent && isMovable())
                move_faked_activity = workspace()->fakeRequestedActivity( this );
        // fallthrough
        case Options::MouseMove:
        case Options::MouseUnrestrictedMove:
            {
            if (!isMovable())
                break;
            if( moveResizeMode )
                finishMoveResize( false );
            mode = PositionCenter;
            buttonDown = TRUE;
            moveOffset = QPoint( globalPos.x() - x(), globalPos.y() - y()); // map from global
            invertedMoveOffset = rect().bottomRight() - moveOffset;
            unrestrictedMoveResize = ( command == Options::MouseActivateRaiseAndUnrestrictedMove
                                    || command == Options::MouseUnrestrictedMove );
            setCursor( mode );
            if( !startMoveResize())
                {
                buttonDown = false;
                setCursor( mode );
                }
            break;
            }
        case Options::MouseResize:
        case Options::MouseUnrestrictedResize:
            {
            if (!isResizable() || isShade())
                break;
            if( moveResizeMode )
                finishMoveResize( false );
            buttonDown = TRUE;
            moveOffset = QPoint( globalPos.x() - x(), globalPos.y() - y()); // map from global
            int x = moveOffset.x(), y = moveOffset.y();
            bool left = x < width() / 3;
            bool right = x >= 2 * width() / 3;
            bool top = y < height() / 3;
            bool bot = y >= 2 * height() / 3;
            if (top)
                mode = left ? PositionTopLeft : (right ? PositionTopRight : PositionTop);
            else if (bot)
                mode = left ? PositionBottomLeft : (right ? PositionBottomRight : PositionBottom);
            else
                mode = (x < width() / 2) ? PositionLeft : PositionRight;
            invertedMoveOffset = rect().bottomRight() - moveOffset;
            unrestrictedMoveResize = ( command == Options::MouseUnrestrictedResize );
            setCursor( mode );
            if( !startMoveResize())
                {
                buttonDown = false;
                setCursor( mode );
                }
            break;
            }
        case Options::MouseMinimize:
            minimize();
            break;
        case Options::MouseNothing:
        // fall through
        default:
            replay = TRUE;
            break;
        }
    return replay;
    }

// KDE4 remove me
void Workspace::showWindowMenuAt( unsigned long, int, int )
    {
    slotWindowOperations();
    }

void Workspace::slotActivateAttentionWindow()
    {
    if( attention_chain.count() > 0 )
        activateClient( attention_chain.first());
    }

void Workspace::slotSwitchDesktopNext()
    {
    int d = currentDesktop() + 1;
     if ( d > numberOfDesktops() ) 
        {
        if ( options->rollOverDesktops ) 
            {
            d = 1;
            }
        else 
            {
            return;
            }
        }
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchDesktopPrevious()
    {
    int d = currentDesktop() - 1;
    if ( d <= 0 ) 
        {
        if ( options->rollOverDesktops )
          d = numberOfDesktops();
      else
          return;
        }
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchDesktopRight()
    {
    int desktop = desktopToRight( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchDesktopLeft()
    {
    int desktop = desktopToLeft( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchDesktopUp()
    {
    int desktop = desktopUp( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchDesktopDown()
    {
    int desktop = desktopDown( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::slotSwitchToDesktop( int i )
    {
    setCurrentDesktop( i );
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }


void Workspace::slotWindowToDesktop( int i )
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( i >= 1 && i <= numberOfDesktops() && c
        && !c->isDesktop()
        && !c->isDock()
        && !c->isTopMenu())
            sendClientToDesktop( c, i, true );
    }

/*!
  Maximizes the popup client
 */
void Workspace::slotWindowMaximize()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if ( c )
        performWindowOperation( c, Options::MaximizeOp );
    }

/*!
  Maximizes the popup client vertically
 */
void Workspace::slotWindowMaximizeVertical()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if ( c )
        performWindowOperation( c, Options::VMaximizeOp );
    }

/*!
  Maximizes the popup client horiozontally
 */
void Workspace::slotWindowMaximizeHorizontal()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if ( c )
        performWindowOperation( c, Options::HMaximizeOp );
    }


/*!
  Minimizes the popup client
 */
void Workspace::slotWindowMinimize()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    performWindowOperation( c, Options::MinimizeOp );
    }

/*!
  Shades/unshades the popup client respectively
 */
void Workspace::slotWindowShade()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    performWindowOperation( c, Options::ShadeOp );
    }

/*!
  Raises the popup client
 */
void Workspace::slotWindowRaise()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if ( c )
        raiseClient( c );
    }

/*!
  Lowers the popup client
 */
void Workspace::slotWindowLower()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if ( c )
        lowerClient( c );
    }

/*!
  Does a toggle-raise-and-lower on the popup client;
  */
void Workspace::slotWindowRaiseOrLower()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if  ( c )
        raiseOrLowerClient( c );
    }

void Workspace::slotWindowOnAllDesktops()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        c->setOnAllDesktops( !c->isOnAllDesktops());
    }

void Workspace::slotWindowFullScreen()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        performWindowOperation( c, Options::FullScreenOp );
    }

void Workspace::slotWindowNoBorder()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        performWindowOperation( c, Options::NoBorderOp );
    }

void Workspace::slotWindowAbove()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        performWindowOperation( c, Options::KeepAboveOp );
    }

void Workspace::slotWindowBelow()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        performWindowOperation( c, Options::KeepBelowOp );
    }
void Workspace::slotSetupWindowShortcut()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    if( c )
        performWindowOperation( c, Options::SetupWindowShortcutOp );
    }

/*!
  Move window to next desktop
 */
void Workspace::slotWindowToNextDesktop()
    {
    int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() )
        d = 1;
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowToPreviousDesktop()
    {
    int d = currentDesktop() - 1;
    if ( d <= 0 )
        d = numberOfDesktops();
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }

void Workspace::slotWindowToDesktopRight()
    {
    int d = desktopToRight( currentDesktop());
    if( d == currentDesktop())
        return;
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }

void Workspace::slotWindowToDesktopLeft()
    {
    int d = desktopToLeft( currentDesktop());
    if( d == currentDesktop())
        return;
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }

void Workspace::slotWindowToDesktopUp()
    {
    int d = desktopUp( currentDesktop());
    if( d == currentDesktop())
        return;
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }

void Workspace::slotWindowToDesktopDown()
    {
    int d = desktopDown( currentDesktop());
    if( d == currentDesktop())
        return;
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        popupinfo->showInfo( desktopName(currentDesktop()) );
        }
    }


/*!
  Kill Window feature, similar to xkill
 */
void Workspace::slotKillWindow()
    {
    KillWindow kill( this );
    kill.start();
    }

/*!
  Sends the popup client to desktop \a desk

  Internal slot for the window operation menu
 */
void Workspace::slotSendToDesktop( int desk )
    {
    if ( !active_popup_client )
        return;
    if ( desk == 0 ) 
        { // the 'on_all_desktops' menu entry
        active_popup_client->setOnAllDesktops( !active_popup_client->isOnAllDesktops());
        return;
        }

    sendClientToDesktop( active_popup_client, desk, false );

    }

/*!
  Shows the window operations popup menu for the activeClient()
 */
void Workspace::slotWindowOperations()
    {
    if ( !active_client )
        return;
    QPoint pos = active_client->pos() + active_client->clientPos();
    showWindowMenu( pos.x(), pos.y(), active_client );
    }

void Workspace::showWindowMenu( const QRect &pos, Client* cl )
    {
    if (!kapp->authorizeKAction("kwin_rmb"))
        return;
    if( !cl )
        return;
    if( active_popup_client != NULL ) // recursion
        return;
    if ( cl->isDesktop()
        || cl->isDock()
        || cl->isTopMenu())
        return;

    active_popup_client = cl;
    QPopupMenu* p = clientPopup();
    active_popup = p;
    int x = pos.left();
    int y = pos.bottom();
    if (y == pos.top())
	p->exec( QPoint( x, y ) );
    else
        {
	QRect area = clientArea(ScreenArea, QPoint(x, y), currentDesktop());
	int popupHeight = p->sizeHint().height();
	if (y + popupHeight < area.height())
	    p->exec( QPoint( x, y ) );
	else
	    p->exec( QPoint( x, pos.top() - popupHeight ) );
        }
    // active popup may be already changed (e.g. the window shortcut dialog)
    if( active_popup == p )
        closeActivePopup();
    }

/*!
  Closes the popup client
 */
void Workspace::slotWindowClose()
    {
    if ( tab_box->isVisible() || popupinfo->isVisible() )
        return;
    Client* c = active_popup_client ? active_popup_client : active_client;
    performWindowOperation( c, Options::CloseOp );
    }

/*!
  Starts keyboard move mode for the popup client
 */
void Workspace::slotWindowMove()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    performWindowOperation( c, Options::UnrestrictedMoveOp );
    }

/*!
  Starts keyboard resize mode for the popup client
 */
void Workspace::slotWindowResize()
    {
    Client* c = active_popup_client ? active_popup_client : active_client;
    performWindowOperation( c, Options::UnrestrictedResizeOp );
    }

void Client::setShortcut( const QString& _cut )
    {
    QString cut = rules()->checkShortcut( _cut );
    if( cut.isEmpty())
        return setShortcutInternal( KShortcut());
// Format:
// base+(abcdef)<space>base+(abcdef)
// E.g. Alt+Ctrl+(ABCDEF) Win+X,Win+(ABCDEF)
    if( !cut.contains( '(' ) && !cut.contains( ')' ) && !cut.contains( ' ' ))
        {
        if( workspace()->shortcutAvailable( KShortcut( cut ), this ))
            setShortcutInternal( KShortcut( cut ));
        else
            setShortcutInternal( KShortcut());
        return;
        }
    QValueList< KShortcut > keys;
    QStringList groups = QStringList::split( ' ', cut );
    for( QStringList::ConstIterator it = groups.begin();
         it != groups.end();
         ++it )
        {
        QRegExp reg( "(.*\\+)\\((.*)\\)" );
        if( reg.search( *it ) > -1 )
            {
            QString base = reg.cap( 1 );
            QString list = reg.cap( 2 );
            for( unsigned int i = 0;
                 i < list.length();
                 ++i )
                {
                KShortcut c( base + list[ i ] );
                if( !c.isNull())
                    keys.append( c );
                }
            }
        }
    for( QValueList< KShortcut >::ConstIterator it = keys.begin();
         it != keys.end();
         ++it )
        {
        if( _shortcut == *it ) // current one is in the list
            return;
        }
    for( QValueList< KShortcut >::ConstIterator it = keys.begin();
         it != keys.end();
         ++it )
        {
        if( workspace()->shortcutAvailable( *it, this ))
            {
            setShortcutInternal( *it );
            return;
            }
        }
    setShortcutInternal( KShortcut());
    }

void Client::setShortcutInternal( const KShortcut& cut )
    {
    if( _shortcut == cut )
        return;
    _shortcut = cut;
    updateCaption();
    workspace()->clientShortcutUpdated( this );
    }

bool Workspace::shortcutAvailable( const KShortcut& cut, Client* ignore ) const
    {
    // TODO check global shortcuts etc.
    for( ClientList::ConstIterator it = clients.begin();
         it != clients.end();
         ++it )
        {
        if( (*it) != ignore && (*it)->shortcut() == cut )
            return false;    
        }
    return true;
    }

} // namespace
