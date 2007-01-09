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
#include <QPushButton>
#include <QSlider>
#include <QToolTip>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobalaccel.h>
#include <kapplication.h>
#include <QRegExp>
#include <QMenu>
#include <QVBoxLayout>
#include <kauthorized.h>
#include <kactioncollection.h>
#include <kaction.h>

#include "killwindow.h"
#include "tabbox.h"

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

QMenu* Workspace::clientPopup()
    {
    if ( !popup )
        {
        popup = new QMenu;
        popup->setFont(KGlobalSettings::menuFont());
        connect( popup, SIGNAL( aboutToShow() ), this, SLOT( clientPopupAboutToShow() ) );
        connect( popup, SIGNAL( triggered(QAction*) ), this, SLOT( clientPopupActivated(QAction*) ) );

        advanced_popup = new QMenu( popup );
        advanced_popup->setFont(KGlobalSettings::menuFont());

        mKeepAboveOpAction = advanced_popup->addAction( i18n("Keep &Above Others") );
        mKeepAboveOpAction->setIcon( SmallIconSet( "up" ) );
        KAction *kaction = qobject_cast<KAction*>( keys->action("Window Above Other Windows") );
        if ( kaction!=0 )
            mKeepAboveOpAction->setShortcut( kaction->globalShortcut().primary() );
        mKeepAboveOpAction->setCheckable( true );
        mKeepAboveOpAction->setData( Options::KeepAboveOp );

        mKeepBelowOpAction = advanced_popup->addAction( i18n("Keep &Below Others") );
        mKeepBelowOpAction->setIcon( SmallIconSet( "down" ) );
        kaction = qobject_cast<KAction*>( keys->action("Window Below Other Windows") );
        if ( kaction!=0 )
            mKeepBelowOpAction->setShortcut( kaction->globalShortcut().primary() );
        mKeepBelowOpAction->setCheckable( true );
        mKeepBelowOpAction->setData( Options::KeepBelowOp );

        mFullScreenOpAction = advanced_popup->addAction( i18n("&Fullscreen") );
        mFullScreenOpAction->setIcon( SmallIconSet( "window_fullscreen" ) );
        kaction = qobject_cast<KAction*>( keys->action("Window Fullscreen") );
        if ( kaction!=0 )
            mFullScreenOpAction->setShortcut( kaction->globalShortcut().primary() );
        mFullScreenOpAction->setCheckable( true );
        mFullScreenOpAction->setData( Options::FullScreenOp );

        mNoBorderOpAction = advanced_popup->addAction( i18n("&No Border") );
        kaction = qobject_cast<KAction*>( keys->action("Window No Border") );
        if ( kaction!=0 )
            mNoBorderOpAction->setShortcut( kaction->globalShortcut().primary() );
        mNoBorderOpAction->setCheckable( true );
        mNoBorderOpAction->setData( Options::NoBorderOp );

        QAction *action = advanced_popup->addAction( i18n("Window &Shortcut...") );
        action->setIcon( SmallIconSet("key_bindings") );
        kaction = qobject_cast<KAction*>( keys->action("Setup Window Shortcut") );
        if ( kaction!=0 )
            action->setShortcut( kaction->globalShortcut().primary() );
        action->setData( Options::SetupWindowShortcutOp );

        action = advanced_popup->addAction( i18n("&Special Window Settings...") );
        action->setIcon( SmallIconSet( "wizard" ) );
        action->setData( Options::WindowRulesOp );

        action = advanced_popup->addAction( i18n("&Special Application Settings...") );
        action->setIcon( SmallIconSet( "wizard" ) );
        action->setData( Options::ApplicationRulesOp );

        action = popup->addMenu( advanced_popup );
        action->setText( i18n("Ad&vanced") );

        desk_popup_index = popup->actions().count();

        if (options->useTranslucency){
            trans_popup = new QMenu( popup );
            trans_popup->setFont(KGlobalSettings::menuFont());
            connect( trans_popup, SIGNAL( triggered(QAction*) ), this, SLOT( setPopupClientOpacity(QAction*)));
            const int levels[] = { 100, 90, 75, 50, 25, 10 };
            for( unsigned int i = 0;
                 i < sizeof( levels ) / sizeof( levels[ 0 ] );
                 ++i )
                {
                action = trans_popup->addAction( QString::number( levels[ i ] ) + "%" );
                action->setCheckable( true );
                action->setData( levels[ i ] );
                }
            action = popup->addMenu( trans_popup );
            action->setText( i18n("&Opacity") );
        }

        mMoveOpAction = popup->addAction( i18n("&Move") );
        mMoveOpAction->setIcon( SmallIconSet( "move" ) );
        kaction = qobject_cast<KAction*>( keys->action("Window Move") );
        if ( kaction!=0 )
            mMoveOpAction->setShortcut( kaction->globalShortcut().primary() );
        mMoveOpAction->setData( Options::MoveOp );

        mResizeOpAction = popup->addAction( i18n("Re&size") );
        kaction = qobject_cast<KAction*>( keys->action("Window Resize") );
        if ( kaction!=0 )
            mResizeOpAction->setShortcut( kaction->globalShortcut().primary() );
        mResizeOpAction->setData( Options::ResizeOp );

        mMinimizeOpAction = popup->addAction( i18n("Mi&nimize") );
        kaction = qobject_cast<KAction*>( keys->action("Window Minimize") );
        if ( kaction!=0 )
            mMinimizeOpAction->setShortcut( kaction->globalShortcut().primary() );
        mMinimizeOpAction->setData( Options::MinimizeOp );

        mMaximizeOpAction = popup->addAction( i18n("Ma&ximize") );
        kaction = qobject_cast<KAction*>( keys->action("Window Maximize") );
        if ( kaction!=0 )
            mMaximizeOpAction->setShortcut( kaction->globalShortcut().primary() );
        mMaximizeOpAction->setCheckable( true );
        mMaximizeOpAction->setData( Options::MaximizeOp );

        mShadeOpAction = popup->addAction( i18n("Sh&ade") );
        kaction = qobject_cast<KAction*>( keys->action("Window Shade") );
        if ( kaction!=0 )
            mShadeOpAction->setShortcut( kaction->globalShortcut().primary() );
        mShadeOpAction->setCheckable( true );
        mShadeOpAction->setData( Options::ShadeOp );

        popup->addSeparator();

        if (!KGlobal::config()->isImmutable() &&
            !KAuthorized::authorizeControlModules(Workspace::configModules(true)).isEmpty())
            {
              action = popup->addAction( i18n("Configur&e Window Behavior...") );
              action->setIcon( SmallIconSet( "configure" ) );
              connect( action, SIGNAL( triggered() ), this, SLOT( configureWM() ) );
              popup->addSeparator();
            }

        mCloseOpAction = popup->addAction( i18n("&Close") );
        mCloseOpAction->setIcon( SmallIconSet( "fileclose" ) );
        kaction = qobject_cast<KAction*>( keys->action("Window Close") );
        if ( kaction!=0 )
            mCloseOpAction->setShortcut( kaction->globalShortcut().primary() );
        mCloseOpAction->setData( Options::CloseOp );
        }
    return popup;
    }

void Workspace::setPopupClientOpacity( QAction* action )
    {
    if( active_popup_client == NULL )
        return;
    int level = action->data().toInt();
    active_popup_client->setOpacity( level / 100.0 );
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

    mResizeOpAction->setEnabled( active_popup_client->isResizable() );
    mMoveOpAction->setEnabled( active_popup_client->isMovable() );
    mMaximizeOpAction->setEnabled( active_popup_client->isMaximizable() );
    mMaximizeOpAction->setChecked( active_popup_client->maximizeMode() == Client::MaximizeFull );
    mShadeOpAction->setEnabled( active_popup_client->isShadeable() );
    mShadeOpAction->setChecked( active_popup_client->shadeMode() != ShadeNone );
    mKeepAboveOpAction->setChecked( active_popup_client->keepAbove() );
    mKeepBelowOpAction->setChecked( active_popup_client->keepBelow() );
    mFullScreenOpAction->setEnabled( active_popup_client->userCanSetFullScreen() );
    mFullScreenOpAction->setChecked( active_popup_client->isFullScreen() );
    mNoBorderOpAction->setEnabled( active_popup_client->userCanSetNoBorder() );
    mNoBorderOpAction->setChecked( active_popup_client->noBorder() );
    mMinimizeOpAction->setEnabled( active_popup_client->isMinimizable() );
    mCloseOpAction->setEnabled( active_popup_client->isCloseable() );
    if (options->useTranslucency)
        {
        foreach( QAction* action, trans_popup->actions())
            {
            if( action->data().toInt() == qRound( active_popup_client->opacity() * 100 ))
                action->setChecked( true );
            else
                action->setChecked( false );
            }
        }
    }


void Workspace::initDesktopPopup()
    {
    if (desk_popup)
        return;

    desk_popup = new QMenu( popup );
    desk_popup->setFont(KGlobalSettings::menuFont());
    connect( desk_popup, SIGNAL( triggered(QAction*) ),
             this, SLOT( slotSendToDesktop(QAction*) ) );
    connect( desk_popup, SIGNAL( aboutToShow() ),
             this, SLOT( desktopPopupAboutToShow() ) );

    QAction *action = popup->addMenu( desk_popup );
    action->setText( i18n("To &Desktop") );
    action->setData( desk_popup_index );
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
    QAction *action = desk_popup->addAction( i18n("&All Desktops") );
    action->setData( 0 );
    action->setCheckable( true );

    if ( active_popup_client && active_popup_client->isOnAllDesktops() )
        action->setChecked( true );
    desk_popup->addSeparator();

    const int BASE = 10;
    for ( int i = 1; i <= numberOfDesktops(); i++ ) {
        QString basic_name("%1  %2");
        if (i<BASE) {
            basic_name.prepend('&');
        }
        action = desk_popup->addAction( basic_name.arg(i).arg( desktopName(i).replace( '&', "&&" ) ) );
        action->setData( i );
        action->setCheckable( true );

        if ( active_popup_client &&
             !active_popup_client->isOnAllDesktops() && active_popup_client->desktop()  == i )
            action->setChecked( true );
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
    keys = new KActionCollection( this );
    KActionCollection* actionCollection = keys;
    QAction* a = 0L;

    // a separate KActionCollection is needed for the shortcut for disabling global shortcuts,
    // otherwise it would also disable itself
    disable_shortcuts_keys = new KActionCollection( this );
    // FIXME KAccel port... needed?
    //disable_shortcuts_keys->disableBlocking( true );
#define IN_KWIN
#include "kwinbindings.cpp"
    readShortcuts();
    }

void Workspace::readShortcuts()
    {
    KGlobalAccel::self()->readSettings();

    KAction *kaction = qobject_cast<KAction*>( keys->action("Walk Through Desktops") );
    if ( kaction!=0 )
        cutWalkThroughDesktops = kaction->globalShortcut();

    kaction = qobject_cast<KAction*>( keys->action("Walk Through Desktops (Reverse)") );
    if ( kaction!=0 )
        cutWalkThroughDesktopsReverse = kaction->globalShortcut();

    kaction = qobject_cast<KAction*>( keys->action("Walk Through Desktop List") );
    if ( kaction!=0 )
        cutWalkThroughDesktopList = kaction->globalShortcut();

    kaction = qobject_cast<KAction*>( keys->action("Walk Through Desktop List (Reverse)") );
    if ( kaction!=0 )
        cutWalkThroughDesktopListReverse = kaction->globalShortcut();

    kaction = qobject_cast<KAction*>( keys->action("Walk Through Windows") );
    if ( kaction!=0 )
        cutWalkThroughWindows = kaction->globalShortcut();

    kaction = qobject_cast<KAction*>( keys->action("Walk Through Windows (Reverse)") );
    if ( kaction!=0 )
        cutWalkThroughWindowsReverse = kaction->globalShortcut();

    delete popup;
    popup = NULL; // so that it's recreated next time
    desk_popup = NULL;
    }


void Workspace::setupWindowShortcut( Client* c )
    {
    assert( client_keys_dialog == NULL );
    keys->setEnabled( false );
    disable_shortcuts_keys->setEnabled( false );
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
    disable_shortcuts_keys->setEnabled( true );
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
    QAction* action = client_keys->action( key.toLatin1().constData() );
    if( !c->shortcut().isEmpty())
        {
        action->setShortcuts(c->shortcut());
        connect(action, SIGNAL(triggered(bool)), c, SLOT(shortcutActivated()));
        action->setEnabled( true );
        }
    else
        {
        delete action;
        }
    }

void Workspace::clientPopupActivated( QAction *action )
    {
      if ( !action->data().isValid() )
        return;

    WindowOperation op = static_cast< WindowOperation >( action->data().toInt() );
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
            c->performMouseCommand( Options::MouseMove, cursorPos() );
            break;
        case Options::UnrestrictedMoveOp:
            c->performMouseCommand( Options::MouseUnrestrictedMove, cursorPos() );
            break;
        case Options::ResizeOp:
            c->performMouseCommand( Options::MouseResize, cursorPos() );
            break;
        case Options::UnrestrictedResizeOp:
            c->performMouseCommand( Options::MouseUnrestrictedResize, cursorPos() );
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
        case Options::RestoreOp:
            c->maximize( Client::MaximizeRestore );
            break;
        case Options::MinimizeOp:
            c->minimize();
            break;
        case Options::ShadeOp:
            c->performMouseCommand( Options::MouseShade, cursorPos());
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
            {
            StackingUpdatesBlocker blocker( this );
            bool was = c->keepAbove();
            c->setKeepAbove( !c->keepAbove() );
            if( was && !c->keepAbove())
                raiseClient( c );
            break;
            }
        case Options::KeepBelowOp:
            {
            StackingUpdatesBlocker blocker( this );
            bool was = c->keepBelow();
            c->setKeepBelow( !c->keepBelow() );
            if( was && !c->keepBelow())
                lowerClient( c );
            break;
            }
        case Options::OperationsOp:
            c->performMouseCommand( Options::MouseShade, cursorPos());
            break;
        case Options::WindowRulesOp:
            editWindowRules( c, false );
            break;
        case Options::ApplicationRulesOp:
            editWindowRules( c, true );
            break;
        case Options::SetupWindowShortcutOp:
            setupWindowShortcut( c );
            break;
        case Options::LowerOp:
            lowerClient(c);
            break;
        case Options::NoOp:
            break;
        }
    }

/*!
  Performs a mouse command on this client (see options.h)
 */
bool Client::performMouseCommand( Options::MouseCommand command, QPoint globalPos, bool handled )
    {
    bool replay = false;
    switch (command) 
        {
        case Options::MouseRaise:
            workspace()->raiseClient( this );
            break;
        case Options::MouseLower:
            workspace()->lowerClient( this );
            break;
        case Options::MouseShade :
            toggleShade();
            cancelShadeHover();
            break;
        case Options::MouseSetShade:
            setShade( ShadeNormal );
            cancelShadeHover();
            break;
        case Options::MouseUnsetShade:
            setShade( ShadeNone );
            cancelShadeHover();
            break;
        case Options::MouseOperationsMenu:
            if ( isActive() && options->clickRaise )
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
            replay = true;
            break;
        case Options::MouseActivateAndPassClick:
            workspace()->takeActivity( this, ActivityFocus, handled );
            replay = true;
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
            buttonDown = true;
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
            buttonDown = true;
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
        case Options::MouseMaximize:
            maximize( Client::MaximizeFull );
            break;
        case Options::MouseRestore:
            maximize( Client::MaximizeRestore );
            break;
        case Options::MouseMinimize:
            minimize();
            break;
        case Options::MouseAbove:
            {
            StackingUpdatesBlocker blocker( workspace());
            if( keepBelow())
                setKeepBelow( false );
            else
                setKeepAbove( true );
            break;
            }
        case Options::MouseBelow:
            {
            StackingUpdatesBlocker blocker( workspace());
            if( keepAbove())
                setKeepAbove( false );
            else
                setKeepBelow( true );
            break;
            }
        case Options::MousePreviousDesktop:
            workspace()->windowToPreviousDesktop( this );
            break;
        case Options::MouseNextDesktop:
            workspace()->windowToNextDesktop( this );
            break;
        case Options::MouseOpacityMore:
            setOpacity( qMin( opacity() + 0.1, 1.0 ));
            break;
        case Options::MouseOpacityLess:
            setOpacity( qMax( opacity() - 0.1, 0.0 ));
            break;
        case Options::MouseNothing:
            replay = true;
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
    }

void Workspace::slotSwitchDesktopRight()
    {
    int desktop = desktopToRight( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    }

void Workspace::slotSwitchDesktopLeft()
    {
    int desktop = desktopToLeft( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    }

void Workspace::slotSwitchDesktopUp()
    {
    int desktop = desktopUp( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    }

void Workspace::slotSwitchDesktopDown()
    {
    int desktop = desktopDown( currentDesktop());
    if( desktop == currentDesktop())
        return;
    setCurrentDesktop( desktop );
    }

void Workspace::slotSwitchToDesktop( int i )
    {
    setCurrentDesktop( i );
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
    windowToNextDesktop( active_popup_client ? active_popup_client : active_client );
    }
    
void Workspace::windowToNextDesktop( Client* c )
    {
    int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() )
        d = 1;
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
        }
    }

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowToPreviousDesktop()
    {
    windowToPreviousDesktop( active_popup_client ? active_popup_client : active_client );
    }
    
void Workspace::windowToPreviousDesktop( Client* c )
    {
    int d = currentDesktop() - 1;
    if ( d <= 0 )
        d = numberOfDesktops();
    if (c && !c->isDesktop()
        && !c->isDock() && !c->isTopMenu())
        {
        setClientIsMoving( c );
        setCurrentDesktop( d );
        setClientIsMoving( NULL );
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
void Workspace::slotSendToDesktop( QAction *action )
    {
      int desk = action->data().toInt();
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
    if (!KAuthorized::authorizeKAction("kwin_rmb"))
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
    QMenu* p = clientPopup();
    active_popup = p;
    int x = pos.left();
    int y = pos.bottom();
    if (y == pos.top())
	p->exec( QPoint( x, y ) );
    else
        {
	QRect area = clientArea(ScreenArea, QPoint(x, y), currentDesktop());
        clientPopupAboutToShow(); // needed for sizeHint() to be correct :-/
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
    if ( tab_box->isVisible())
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
    QList< KShortcut > keys;
    QStringList groups = cut.split( ' ');
    for( QStringList::ConstIterator it = groups.begin();
         it != groups.end();
         ++it )
        {
        QRegExp reg( "(.*\\+)\\((.*)\\)" );
        if( reg.indexIn( *it ) > -1 )
            {
            QString base = reg.cap( 1 );
            QString list = reg.cap( 2 );
            for( int i = 0;
                 i < list.length();
                 ++i )
                {
                KShortcut c( base + list[ i ] );
                if( !c.isEmpty())
                    keys.append( c );
                }
            }
        }
    for( QList< KShortcut >::ConstIterator it = keys.begin();
         it != keys.end();
         ++it )
        {
        if( _shortcut == *it ) // current one is in the list
            return;
        }
    for( QList< KShortcut >::ConstIterator it = keys.begin();
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
