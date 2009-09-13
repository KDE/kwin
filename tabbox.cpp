/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

//#define QT_CLEAN_NAMESPACE
// own
#include "tabbox.h"
// tabbox
#include "tabbox/clientmodel.h"
#include "tabbox/desktopmodel.h"
#include "tabbox/tabboxconfig.h"
// kwin
#include "client.h"
#include "effects.h"
#include "workspace.h"
// Qt
#include <QAction>
#include <QX11Info>
// KDE
#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <kkeyserver.h>
// X11
#include <fixx11h.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

// specify externals before namespace

namespace KWin
{

extern QPixmap* kwin_get_menu_pix_hack();

namespace TabBox
{

TabBoxHandlerImpl::TabBoxHandlerImpl()
    : TabBoxHandler()
    {
    }

TabBoxHandlerImpl::~TabBoxHandlerImpl()
    {
    }

int TabBoxHandlerImpl::activeScreen() const
    {
    return Workspace::self()->activeScreen();
    }

int TabBoxHandlerImpl::currentDesktop() const
    {
    return Workspace::self()->currentDesktop();
    }

QString TabBoxHandlerImpl::desktopName( TabBoxClient* client ) const
    {
    if( TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >( client ) )
        {
        if( !c->client()->isOnAllDesktops() )
            return Workspace::self()->desktopName( c->client()->desktop() );
        }
    return Workspace::self()->desktopName( Workspace::self()->currentDesktop() );
    }

QString TabBoxHandlerImpl::desktopName( int desktop ) const
    {
    return Workspace::self()->desktopName( desktop );
    }

TabBoxClient* TabBoxHandlerImpl::nextClientFocusChain( TabBoxClient* client ) const
    {
    if( TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >( client ) )
        {
        Client* next = Workspace::self()->nextClientFocusChain( c->client() );
        if( next )
            return next->tabBoxClient();
        }
    return NULL;
    }

int TabBoxHandlerImpl::nextDesktopFocusChain( int desktop ) const
    {
    return Workspace::self()->nextDesktopFocusChain( desktop );
    }

int TabBoxHandlerImpl::numberOfDesktops() const
    {
    return Workspace::self()->numberOfDesktops();
    }

TabBoxClient* TabBoxHandlerImpl::activeClient() const
    {
    if( Workspace::self()->activeClient() )
        return Workspace::self()->activeClient()->tabBoxClient();
    else
        return NULL;
    }

TabBoxClient* TabBoxHandlerImpl::clientToAddToList( TabBoxClient* client, int desktop, bool allDesktops ) const
    {
    Workspace* workspace = Workspace::self();
    Client* ret = NULL;
    Client* current = (static_cast< TabBoxClientImpl* >( client ))->client();
    bool addClient = false;
    if( allDesktops )
        addClient = true;
    else
        addClient = current->isOnDesktop( desktop );
    addClient = addClient && current->wantsTabFocus();
    if ( addClient )
        { // don't add windows that have modal dialogs
        Client* modal = current->findModal();
        if( modal == NULL || modal == current )
            ret = current;
        else if( clientList().contains( modal->tabBoxClient() ) )
            ret = modal;
        else
            {
            // nothing
            }
        }
    if( options->separateScreenFocus && options->xineramaEnabled )
        {
        if( current->screen() != workspace->activeScreen())
            ret = NULL;
        }
    if( ret )
        return ret->tabBoxClient();
    else
        return NULL;
    }

TabBoxClientList TabBoxHandlerImpl::stackingOrder() const
    {
    ClientList stacking = Workspace::self()->stackingOrder();
    TabBoxClientList ret;
    foreach( const Client* client, stacking )
        {
        ret.append( client->tabBoxClient() );
        }
    return ret;
    }

/*********************************************************
* TabBoxClientImpl
*********************************************************/

TabBoxClientImpl::TabBoxClientImpl()
    : TabBoxClient()
    {
    }

TabBoxClientImpl::~TabBoxClientImpl()
    {
    }

QString TabBoxClientImpl::caption() const
    {
    return m_client->caption();
    }

QPixmap TabBoxClientImpl::icon() const
    {
    return m_client->icon();
    }

WId TabBoxClientImpl::window() const
    {
    return m_client->window();
    }

bool TabBoxClientImpl::isMinimized() const
    {
    return m_client->isMinimized();
    }

int TabBoxClientImpl::x() const
    {
    return m_client->x();
    }

int TabBoxClientImpl::y() const
    {
    return m_client->y();
    }

int TabBoxClientImpl::width() const
    {
    return m_client->width();
    }

int TabBoxClientImpl::height() const
    {
    return m_client->height();
    }


/*********************************************************
* TabBox
*********************************************************/
TabBox::TabBox( Workspace *ws )
    : QObject()
    , wspace(ws)
    , display_refcount( 0 )
    {
    m_isShown = false;
    m_defaultConfig = TabBoxConfig();
    m_defaultConfig.setTabBoxMode( TabBoxConfig::ClientTabBox );
    m_defaultConfig.setClientListMode( TabBoxConfig::CurrentDesktopClientList );
    m_defaultConfig.setClientSwitchingMode( TabBoxConfig::FocusChainSwitching );
    m_defaultConfig.setLayout( TabBoxConfig::VerticalLayout );

    m_alternativeConfig = TabBoxConfig();
    m_alternativeConfig.setTabBoxMode( TabBoxConfig::ClientTabBox );
    m_alternativeConfig.setClientListMode( TabBoxConfig::AllDesktopsClientList );
    m_alternativeConfig.setClientSwitchingMode( TabBoxConfig::FocusChainSwitching );
    m_alternativeConfig.setLayout( TabBoxConfig::VerticalLayout );

    m_desktopConfig = TabBoxConfig();
    m_desktopConfig.setTabBoxMode( TabBoxConfig::DesktopTabBox );
    m_desktopConfig.setShowTabBox( true );
    m_desktopConfig.setDesktopSwitchingMode( TabBoxConfig::MostRecentlyUsedDesktopSwitching );
    m_desktopConfig.setLayout( TabBoxConfig::VerticalLayout );

    m_desktopListConfig = TabBoxConfig();
    m_desktopListConfig.setTabBoxMode( TabBoxConfig::DesktopTabBox );
    m_desktopListConfig.setShowTabBox( true );
    m_desktopListConfig.setDesktopSwitchingMode( TabBoxConfig::StaticDesktopSwitching );
    m_desktopListConfig.setLayout( TabBoxConfig::VerticalLayout );
    m_tabBox = new TabBoxHandlerImpl();
    m_tabBox->setConfig( m_defaultConfig );


    m_tabBoxMode = TabBoxDesktopMode; // init variables
    reconfigure();
    connect(&delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
    }

TabBox::~TabBox()
    {
    }


/*!
  Sets the current mode to \a mode, either TabBoxDesktopListMode or TabBoxWindowsMode

  \sa mode()
 */
void TabBox::setMode( TabBoxMode mode )
    {
    m_tabBoxMode = mode;
    switch( mode )
        {
        case TabBoxWindowsMode:
            m_tabBox->setConfig( m_defaultConfig );
            break;
        case TabBoxWindowsAlternativeMode:
            m_tabBox->setConfig( m_alternativeConfig );
            break;
        case TabBoxDesktopMode:
            m_tabBox->setConfig( m_desktopConfig );
            break;
        case TabBoxDesktopListMode:
            m_tabBox->setConfig( m_desktopListConfig );
            break;
        }
    }

/*!
  Resets the tab box to display the active client in TabBoxWindowsMode, or the
  current desktop in TabBoxDesktopListMode
 */
void TabBox::reset( bool partial_reset )
    {
    switch( m_tabBox->config().tabBoxMode() )
        {
        case TabBoxConfig::ClientTabBox:
            m_tabBox->createModel( partial_reset );
            if( !partial_reset )
                {
                setCurrentClient( workspace()->activeClient() );
                // it's possible that the active client is not part of the model
                // in that case the index is invalid
                if( !m_index.isValid() )
                    setCurrentIndex( m_tabBox->first() );
                }
            else
                {
                if( !m_index.isValid() || !m_tabBox->client( m_index ) )
                    setCurrentIndex( m_tabBox->first() );
                }
            break;
        case TabBoxConfig::DesktopTabBox:
            m_tabBox->createModel();

            if( !partial_reset )
                setCurrentDesktop( workspace()->currentDesktop() );
            break;
        }

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }

/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev( bool next )
    {
    setCurrentIndex( m_tabBox->nextPrev( next ), false );

    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }


/*!
  Returns the currently displayed client ( only works in TabBoxWindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* TabBox::currentClient()
    {
    if( TabBoxClientImpl* client = static_cast< TabBoxClientImpl* >( m_tabBox->client( m_index )) )
        {
        if( !workspace()->hasClient( client->client() ) )
            return NULL;
        return client->client();
        }
    else
        return NULL;
    }

/*!
  Returns the list of clients potentially displayed ( only works in
  TabBoxWindowsMode ).
  Returns an empty list if no clients are available.
 */
ClientList TabBox::currentClientList()
    {
    TabBoxClientList list = m_tabBox->clientList();
    ClientList ret;
    foreach( const TabBoxClient* client, list )
        {
        if( const TabBoxClientImpl* c = static_cast< const TabBoxClientImpl* >( client ) )
            ret.append( c->client() );
        }
    return ret;
    }


/*!
  Returns the currently displayed virtual desktop ( only works in
  TabBoxDesktopListMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
    {
    return m_tabBox->desktop( m_index );
    }


/*!
  Returns the list of desktops potentially displayed ( only works in
  TabBoxDesktopListMode )
  Returns an empty list if no are available.
 */
QList< int > TabBox::currentDesktopList()
    {
    return m_tabBox->desktopList();
    }


/*!
  Change the currently selected client, and notify the effects.

  \sa setCurrentDesktop()
 */
void TabBox::setCurrentClient( Client* newClient )
    {
    setCurrentIndex( m_tabBox->index( newClient->tabBoxClient() ) );
    }

/*!
  Change the currently selected desktop, and notify the effects.

  \sa setCurrentClient()
 */
void TabBox::setCurrentDesktop( int newDesktop )
    {
    setCurrentIndex( m_tabBox->desktopIndex( newDesktop ) );
    }

void TabBox::TabBox::setCurrentIndex( QModelIndex index, bool notifyEffects )
    {
    if( !index.isValid() )
        return;
    m_index = index;
    m_tabBox->setCurrentIndex( index );
    if( effects && notifyEffects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxUpdated();
    }

/*!
  Notify effects that the tab box is being shown, and only display the
  default tab box QFrame if no effect has referenced the tab box.
*/
void TabBox::show()
    {
    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxAdded( m_tabBoxMode );
    if( isDisplayed())
        {
        m_isShown = false;
        return;
        }
    refDisplay();
    m_isShown = true;
    m_tabBox->show();
    }


/*!
  Notify effects that the tab box is being hidden.
*/
void TabBox::hide()
    {
    delayedShowTimer.stop();
    if( m_isShown )
        {
        m_isShown = false;
        unrefDisplay();
        }
    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->tabBoxClosed();
    if( isDisplayed())
        kDebug( 1212 ) << "Tab box was not properly closed by an effect";
    m_index = QModelIndex();
    m_tabBox->hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (display(), EnterNotify, &otherEvent ) )
        ;
    }


/*!
  Decrease the reference count.  Only when the reference count is 0 will
  the default tab box be shown.
 */
void TabBox::unrefDisplay()
    {
    --display_refcount;
    }

void TabBox::reconfigure()
    {
    KSharedConfigPtr c(KGlobal::config());
    KConfigGroup config = c->group( "TabBox" );

    loadConfig( c->group( "TabBox" ), m_defaultConfig );
    loadConfig( c->group( "TabBoxAlternative" ), m_alternativeConfig );

    m_tabBox->setConfig( m_defaultConfig );

    m_delayShow = config.readEntry<bool>( "ShowDelay", true );
    m_delayShowTime = config.readEntry<int>( "DelayTime", 90 );
    }

void TabBox::loadConfig( const KConfigGroup& config, TabBoxConfig& tabBoxConfig )
    {
    tabBoxConfig.setClientListMode( TabBoxConfig::ClientListMode(
        config.readEntry<int>( "ListMode", TabBoxConfig::defaultListMode() )));
    tabBoxConfig.setClientSwitchingMode( TabBoxConfig::ClientSwitchingMode(
        config.readEntry<int>( "SwitchingMode", TabBoxConfig::defaultSwitchingMode() )));
    tabBoxConfig.setLayout( TabBoxConfig::LayoutMode(
        config.readEntry<int>( "LayoutMode", TabBoxConfig::defaultLayoutMode() )));
    tabBoxConfig.setSelectedItemViewPosition( TabBoxConfig::SelectedItemViewPosition(
        config.readEntry<int>( "SelectedItem", TabBoxConfig::defaultSelectedItemViewPosition())));

    tabBoxConfig.setShowOutline( config.readEntry<bool>( "ShowOutline",
                                                             TabBoxConfig::defaultShowOutline()));
    tabBoxConfig.setShowTabBox( config.readEntry<bool>( "ShowTabBox",
                                                            TabBoxConfig::defaultShowTabBox()));
    tabBoxConfig.setHighlightWindows( config.readEntry<bool>( "HighlightWindows",
                                                                  TabBoxConfig::defaultHighlightWindow()));

    tabBoxConfig.setMinWidth( config.readEntry<int>( "MinWidth",
                                                         TabBoxConfig::defaultMinWidth()));
    tabBoxConfig.setMinHeight( config.readEntry<int>( "MinHeight",
                                                         TabBoxConfig::defaultMinHeight()));

    tabBoxConfig.setLayoutName( config.readEntry<QString>( "LayoutName", TabBoxConfig::defaultLayoutName()));
    tabBoxConfig.setSelectedItemLayoutName( config.readEntry<QString>( "SelectedLayoutName", TabBoxConfig::defaultSelectedItemLayoutName()));
    }

/*!
  Rikkus: please document!   (Matthias)

  Ok, here's the docs :)

  You call delayedShow() instead of show() directly.

  If the 'ShowDelay' setting is false, show() is simply called.

  Otherwise, we start a timer for the delay given in the settings and only
  do a show() when it times out.

  This means that you can alt-tab between windows and you don't see the
  tab box immediately. Not only does this make alt-tabbing faster, it gives
  less 'flicker' to the eyes. You don't need to see the tab box if you're
  just quickly switching between 2 or 3 windows. It seems to work quite
  nicely.
 */
void TabBox::delayedShow()
    {
    if( isDisplayed() || delayedShowTimer.isActive() )
        // already called show - no need to call it twice
        return;

    if( !m_delayShowTime )
        {
        show();
        return;
        }

    delayedShowTimer.setSingleShot(true);
    delayedShowTimer.start( m_delayShowTime );
    }


void TabBox::handleMouseEvent( XEvent* e )
    {
    XAllowEvents( display(), AsyncPointer, xTime());
    if( !m_isShown && isDisplayed())
        { // tabbox has been replaced, check effects
        if( effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent( e ))
            return;
        }
    if( e->type != ButtonPress )
        return;
    QPoint pos( e->xbutton.x_root, e->xbutton.y_root );

    if(( !m_isShown && isDisplayed())
        || (!m_tabBox->containsPos( pos ) &&
        (e->xbutton.button == Button1 || e->xbutton.button == Button2 || e->xbutton.button == Button3)))
        {
        workspace()->closeTabBox();  // click outside closes tab
        return;
        }

    QModelIndex index;
    if( e->xbutton.button == Button1 || e->xbutton.button == Button2 || e->xbutton.button == Button3 )
        {
        index = m_tabBox->indexAt( pos );
        if( e->xbutton.button == Button2 && index.isValid() )
            {
            if( TabBoxClientImpl* client = static_cast< TabBoxClientImpl* >( m_tabBox->client( index )) )
                {
                if( workspace()->hasClient( client->client() ) )
                    {
                    client->client()->closeWindow();
                    return;
                    }
                }
            }
        }
    else
        {
        // mouse wheel event
        index = m_tabBox->nextPrev( e->xbutton.button == Button5 );
        }

    if( index.isValid() )
        setCurrentIndex( index );
    }

void TabBox::TabBox::grabbedKeyEvent( QKeyEvent* event )
    {
    if( !m_isShown && isDisplayed() )
        { // tabbox has been replaced, check effects
        // TODO: pass keyevent to effects
        return;
        }
    setCurrentIndex( m_tabBox->grabbedKeyEvent( event ) );
    }

} // namespace TabBox


//*******************************
// Workspace
//*******************************


/*!
  Handles alt-tab / control-tab
 */

static
bool areKeySymXsDepressed( bool bAll, const uint keySyms[], int nKeySyms )
    {
    char keymap[32];

    kDebug(125) << "areKeySymXsDepressed: " << (bAll ? "all of " : "any of ") << nKeySyms;

    XQueryKeymap( display(), keymap );

    for( int iKeySym = 0; iKeySym < nKeySyms; iKeySym++ )
        {
        uint keySymX = keySyms[ iKeySym ];
        uchar keyCodeX = XKeysymToKeycode( display(), keySymX );
        int i = keyCodeX / 8;
        char mask = 1 << (keyCodeX - (i * 8));

                // Abort if bad index value,
        if( i < 0 || i >= 32 )
                return false;

        kDebug(125) << iKeySym << ": keySymX=0x" << QString::number( keySymX, 16 )
                << " i=" << i << " mask=0x" << QString::number( mask, 16 )
                << " keymap[i]=0x" << QString::number( keymap[i], 16 ) << endl;

                // If ALL keys passed need to be depressed,
        if( bAll )
            {
            if( (keymap[i] & mask) == 0 )
                    return false;
            }
        else
            {
                        // If we are looking for ANY key press, and this key is depressed,
            if( keymap[i] & mask )
                    return true;
            }
        }

        // If we were looking for ANY key press, then none was found, return false,
        // If we were looking for ALL key presses, then all were found, return true.
    return bAll;
    }

static bool areModKeysDepressed( const QKeySequence& seq )
    {
    uint rgKeySyms[10];
    int nKeySyms = 0;
    if( seq.isEmpty())
        return false;
    int mod = seq[seq.count()-1] & Qt::KeyboardModifierMask;

    if ( mod & Qt::SHIFT )
        {
        rgKeySyms[nKeySyms++] = XK_Shift_L;
        rgKeySyms[nKeySyms++] = XK_Shift_R;
        }
    if ( mod & Qt::CTRL )
        {
        rgKeySyms[nKeySyms++] = XK_Control_L;
        rgKeySyms[nKeySyms++] = XK_Control_R;
        }
    if( mod & Qt::ALT )
        {
        rgKeySyms[nKeySyms++] = XK_Alt_L;
        rgKeySyms[nKeySyms++] = XK_Alt_R;
        }
    if( mod & Qt::META )
        {
        // It would take some code to determine whether the Win key
        // is associated with Super or Meta, so check for both.
        // See bug #140023 for details.
        rgKeySyms[nKeySyms++] = XK_Super_L;
        rgKeySyms[nKeySyms++] = XK_Super_R;
        rgKeySyms[nKeySyms++] = XK_Meta_L;
        rgKeySyms[nKeySyms++] = XK_Meta_R;
        }

    return areKeySymXsDepressed( false, rgKeySyms, nKeySyms );
    }

static bool areModKeysDepressed( const KShortcut& cut )
    {
    if( areModKeysDepressed( cut.primary()) || areModKeysDepressed( cut.alternate()) )
        return true;

    return false;
    }

void Workspace::navigatingThroughWindows( bool forward, const KShortcut& shortcut, TabBoxMode mode )
    {
    if ( tab_grab || control_grab )
        return;
    if ( !options->focusPolicyIsReasonable())
        {
        //ungrabXKeyboard(); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows( forward );
        }
    else
        {
        if ( areModKeysDepressed( shortcut ) )
            {
            if ( startKDEWalkThroughWindows( mode) )
                KDEWalkThroughWindows( forward );
            }
        else
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            KDEOneStepThroughWindows( forward, mode );
        }
    }

void Workspace::slotWalkThroughWindows()
    {
    navigatingThroughWindows( true, cutWalkThroughWindows, TabBoxWindowsMode );
    }

void Workspace::slotWalkBackThroughWindows()
    {
    navigatingThroughWindows( false, cutWalkThroughWindowsReverse, TabBoxWindowsMode );
    }

void Workspace::slotWalkThroughWindowsAlternative()
    {
    navigatingThroughWindows( true, cutWalkThroughWindowsAlternative, TabBoxWindowsAlternativeMode );
    }

void Workspace::slotWalkBackThroughWindowsAlternative()
    {
    navigatingThroughWindows( false, cutWalkThroughWindowsAlternativeReverse, TabBoxWindowsAlternativeMode );
    }

void Workspace::slotWalkThroughDesktops()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktops ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktops( true );
        }
    }

void Workspace::slotWalkBackThroughDesktops()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopsReverse ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktops( false );
        }
    }

void Workspace::slotWalkThroughDesktopList()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopList ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktopList( true );
        }
    }

void Workspace::slotWalkBackThroughDesktopList()
    {
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopListReverse ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktopList( false );
        }
    }

void Workspace::slotWalkThroughDesktopsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktops = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughDesktopsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopsReverse = KShortcut( seq );
    }

void Workspace::slotWalkThroughDesktopListKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopList = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughDesktopListKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughDesktopListReverse = KShortcut( seq );
    }

void Workspace::slotWalkThroughWindowsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindows = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughWindowsKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindowsReverse = KShortcut( seq );
    }

void Workspace::slotWalkThroughWindowsAlternativeKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindowsAlternative = KShortcut( seq );
    }

void Workspace::slotWalkBackThroughWindowsAlternativeKeyChanged( const QKeySequence& seq )
    {
    cutWalkThroughWindowsAlternativeReverse = KShortcut( seq );
    }

void Workspace::modalActionsSwitch( bool enabled )
    {
    QList<KActionCollection*> collections;
    collections.append( keys );
    collections.append( disable_shortcuts_keys );
    collections.append( client_keys );
    foreach (KActionCollection* collection, collections)
        foreach (QAction *action, collection->actions())
            action->setEnabled(enabled);
    }

bool Workspace::startKDEWalkThroughWindows( TabBoxMode mode )
    {
    if( !establishTabBoxGrab())
        return false;
    tab_grab = true;
    modalActionsSwitch( false );
    tab_box->setMode( mode );
    tab_box->reset();
    return true;
    }

bool Workspace::startWalkThroughDesktops( TabBoxMode mode )
    {
    if( !establishTabBoxGrab())
        return false;
    control_grab = true;
    modalActionsSwitch( false );
    tab_box->setMode( mode );
    tab_box->reset();
    return true;
    }

bool Workspace::startWalkThroughDesktops()
    {
    return startWalkThroughDesktops( TabBoxDesktopMode );
    }

bool Workspace::startWalkThroughDesktopList()
    {
    return startWalkThroughDesktops( TabBoxDesktopListMode );
    }

void Workspace::KDEWalkThroughWindows( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::walkThroughDesktops( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::CDEWalkThroughWindows( bool forward )
    {
    Client* c = NULL;
// this function find the first suitable client for unreasonable focus
// policies - the topmost one, with some exceptions (can't be keepabove/below,
// otherwise it gets stuck on them)
    Q_ASSERT( block_stacking_updates == 0 );
    for( int i = stacking_order.size() - 1;
         i >= 0 ;
         --i )
        {
        Client* it = stacking_order.at( i );
        if ( it->isOnCurrentDesktop() && !it->isSpecialWindow()
            && it->isShown( false ) && it->wantsTabFocus()
            && !it->keepAbove() && !it->keepBelow())
            {
            c = it;
            break;
            }
        }
    Client* nc = c;
    bool options_traverse_all;
        {
        KConfigGroup group( KGlobal::config(), "TabBox" );
        options_traverse_all = group.readEntry("TraverseAll", false );
        }

    Client* firstClient = 0;
    do
        {
        nc = forward ? nextClientStatic(nc) : previousClientStatic(nc);
        if (!firstClient)
            {
            // When we see our first client for the second time,
            // it's time to stop.
            firstClient = nc;
            }
        else if (nc == firstClient)
            {
            // No candidates found.
            nc = 0;
            break;
            }
        } while (nc && nc != c &&
            (( !options_traverse_all && !nc->isOnDesktop(currentDesktop())) ||
             nc->isMinimized() || !nc->wantsTabFocus() || nc->keepAbove() || nc->keepBelow() ) );
    if (nc)
        {
        if (c && c != nc)
            lowerClient( c );
        if ( options->focusPolicyIsReasonable() )
            {
            activateClient( nc );
            if( nc->isShade() && options->shadeHover )
                nc->setShade( ShadeActivated );
            }
        else
            {
            if( !nc->isOnDesktop( currentDesktop()))
                setCurrentDesktop( nc->desktop());
            raiseClient( nc );
            }
        }
    }

void Workspace::KDEOneStepThroughWindows( bool forward, TabBoxMode mode )
    {
    tab_box->setMode( mode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if( Client* c = tab_box->currentClient() )
        {
        activateClient( c );
        if( c->isShade() && options->shadeHover )
            c->setShade( ShadeActivated );
        }
    }

void Workspace::oneStepThroughDesktops( bool forward, TabBoxMode mode )
    {
    tab_box->setMode( mode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if ( tab_box->currentDesktop() != -1 )
        setCurrentDesktop( tab_box->currentDesktop() );
    }

void Workspace::oneStepThroughDesktops( bool forward )
    {
    oneStepThroughDesktops( forward, TabBoxDesktopMode );
    }

void Workspace::oneStepThroughDesktopList( bool forward )
    {
    oneStepThroughDesktops( forward, TabBoxDesktopListMode );
    }

/*!
  Handles holding alt-tab / control-tab
 */
void Workspace::tabBoxKeyPress( int keyQt )
    {
    bool forward = false;
    bool backward = false;

    if (tab_grab)
        {
        KShortcut forwardShortcut;
        KShortcut backwardShortcut;
        if( tab_box->mode() == TabBoxWindowsMode )
            {
            forwardShortcut = cutWalkThroughWindows;
            backwardShortcut = cutWalkThroughWindowsReverse;
            }
        else
            {
            forwardShortcut = cutWalkThroughWindowsAlternative;
            backwardShortcut = cutWalkThroughWindowsAlternativeReverse;
            }
        forward = forwardShortcut.contains( keyQt );
        backward = backwardShortcut.contains( keyQt );
        if (forward || backward)
            {
            kDebug(125) << "== " << forwardShortcut.toString()
                << " or " << backwardShortcut.toString() << endl;
            KDEWalkThroughWindows( forward );
            }
        }
    else if (control_grab)
        {
        forward = cutWalkThroughDesktops.contains( keyQt ) ||
                  cutWalkThroughDesktopList.contains( keyQt );
        backward = cutWalkThroughDesktopsReverse.contains( keyQt ) ||
                   cutWalkThroughDesktopListReverse.contains( keyQt );
        if (forward || backward)
            walkThroughDesktops(forward);
        }

    if (control_grab || tab_grab)
        {
        if ( ((keyQt & ~Qt::KeyboardModifierMask) == Qt::Key_Escape)
            && !(forward || backward) )
            { // if Escape is part of the shortcut, don't cancel
            closeTabBox();
            }
        else if( !(forward || backward) )
            {
            QKeyEvent* event = new QKeyEvent( QEvent::KeyPress, keyQt & ~Qt::KeyboardModifierMask, Qt::NoModifier );
            tab_box->grabbedKeyEvent( event );
            }
        }
    }

void Workspace::refTabBox()
    {
    if( tab_box )
        tab_box->refDisplay();
    }

void Workspace::unrefTabBox()
    {
    if( tab_box )
        tab_box->unrefDisplay();
    }

void Workspace::closeTabBox()
    {
    removeTabBoxGrab();
    tab_box->hide();
    modalActionsSwitch( true );
    tab_grab = false;
    control_grab = false;
    }

/*!
  Handles alt-tab / control-tab releasing
 */
void Workspace::tabBoxKeyRelease( const XKeyEvent& ev )
    {
    unsigned int mk = ev.state &
        (KKeyServer::modXShift() |
         KKeyServer::modXCtrl() |
         KKeyServer::modXAlt() |
         KKeyServer::modXMeta() );
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for( int i = ShiftMapIndex;
         i <= Mod5MapIndex;
         ++i )
        if(( mk & ( 1 << i )) != 0 )
        {
        if( mod_index >= 0 )
            return;
        mod_index = i;
        }
    bool release = false;
    if( mod_index == -1 )
        release = true;
    else
        {
        XModifierKeymap* xmk = XGetModifierMapping(display());
        for (int i=0; i<xmk->max_keypermod; i++)
            if (xmk->modifiermap[xmk->max_keypermod * mod_index + i]
                == ev.keycode)
                release = true;
        XFreeModifiermap(xmk);
        }
    if( !release )
         return;
    if (tab_grab)
        {
        bool old_control_grab = control_grab;
        Client* c = tab_box->currentClient();
        closeTabBox();
        control_grab = old_control_grab;
        if( c )
            {
            activateClient( c );
            if( c->isShade() && options->shadeHover )
                c->setShade( ShadeActivated );
            }
        }
    if (control_grab)
        {
        bool old_tab_grab = tab_grab;
        int desktop = tab_box->currentDesktop();
        closeTabBox();
        tab_grab = old_tab_grab;
        if ( desktop != -1 )
            {
            setCurrentDesktop( desktop );
            }
        }
    }


int Workspace::nextDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i >= 0 && i+1 < (int)desktop_focus_chain.size() )
            return desktop_focus_chain[i+1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[ 0 ];
    else
            return 1;
    }

int Workspace::previousDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i-1 >= 0 )
            return desktop_focus_chain[i-1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[desktop_focus_chain.size()-1];
    else
            return numberOfDesktops();
    }

int Workspace::nextDesktopStatic( int iDesktop ) const
    {
    int i = ++iDesktop;
    if( i > numberOfDesktops())
        i = 1;
    return i;
    }

int Workspace::previousDesktopStatic( int iDesktop ) const
    {
    int i = --iDesktop;
    if( i < 1 )
        i = numberOfDesktops();
    return i;
    }

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::nextClientFocusChain( Client* c ) const
    {
    if ( global_focus_chain.isEmpty() )
        return 0;
    int pos = global_focus_chain.indexOf( c );
    if ( pos == -1 )
        return global_focus_chain.last();
    if ( pos == 0 )
        return global_focus_chain.last();
    pos--;
    return global_focus_chain[ pos ];
    }

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::previousClientFocusChain( Client* c ) const
    {
    if ( global_focus_chain.isEmpty() )
        return 0;
    int pos = global_focus_chain.indexOf( c );
    if ( pos == -1 )
        return global_focus_chain.first();
    pos++;
    if ( pos == global_focus_chain.count() )
        return global_focus_chain.first();
    return global_focus_chain[ pos ];
    }

/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::nextClientStatic( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    int pos = clients.indexOf( c );
    if ( pos == -1 )
        return clients.first();
    ++pos;
    if ( pos == clients.count() )
        return clients.first();
    return clients[ pos ];
    }
/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::previousClientStatic( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    int pos = clients.indexOf( c );
    if ( pos == -1 )
        return clients.last();
    if ( pos == 0 )
        return clients.last();
    --pos;
    return clients[ pos ];
    }

Client* Workspace::currentTabBoxClient() const
    {
    if( !tab_box )
        return 0;
    return tab_box->currentClient();
    }

ClientList Workspace::currentTabBoxClientList() const
    {
    if( !tab_box )
        return ClientList();
    return tab_box->currentClientList();
    }

int Workspace::currentTabBoxDesktop() const
    {
    if( !tab_box )
        return -1;
    return tab_box->currentDesktop();
    }

QList< int > Workspace::currentTabBoxDesktopList() const
    {
    if( !tab_box )
        return QList< int >();
    return tab_box->currentDesktopList();
    }

void Workspace::setTabBoxClient( Client* c )
    {
    if( tab_box )
        tab_box->setCurrentClient( c );
    }

void Workspace::setTabBoxDesktop( int iDesktop )
    {
    if( tab_box )
        tab_box->setCurrentDesktop( iDesktop );
    }

bool Workspace::establishTabBoxGrab()
    {
    if( !grabXKeyboard())
        return false;
    // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
    // using Alt+Tab while DND (#44972). However force passive grabs on all windows
    // in order to catch MouseRelease events and close the tabbox (#67416).
    // All clients already have passive grabs in their wrapper windows, so check only
    // the active client, which may not have it.
    assert( !forced_global_mouse_grab );
    forced_global_mouse_grab = true;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    return true;
    }

void Workspace::removeTabBoxGrab()
    {
    ungrabXKeyboard();
    assert( forced_global_mouse_grab );
    forced_global_mouse_grab = false;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    }

} // namespace

#include "tabbox.moc"
