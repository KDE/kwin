/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// SELI zmenit doc

/*

 This file contains things relevant to stacking order and layers.

 Design:

 Normal unconstrained stacking order, as requested by the user (by clicking
 on windows to raise them, etc.), is in Workspace::unconstrained_stacking_order.
 That list shouldn't be used at all, except for building
 Workspace::stacking_order. The building is done
 in Workspace::constrainedStackingOrder(). Only Workspace::stackingOrder() should
 be used to get the stacking order, because it also checks the stacking order
 is up to date.
 All clients are also stored in Workspace::clients (except for isDesktop() clients,
 as those are very special, and are stored in Workspace::desktops), in the order
 the clients were created.

 Every window has one layer assigned in which it is. There are 6 layers,
 from bottom : DesktopLayer, BelowLayer, NormalLayer, DockLayer, AboveLayer
 and ActiveLayer (see also NETWM sect.7.10.). The layer a window is in depends
 on the window type, and on other things like whether the window is active.

 NET::Splash clients belong to the Normal layer. NET::TopMenu clients
 belong to Dock layer. Clients that are both NET::Dock and NET::KeepBelow
 are in the Normal layer in order to keep the 'allow window to cover
 the panel' Kicker setting to work as intended (this may look like a slight
 spec violation, but a) I have no better idea, b) the spec allows adjusting
 the stacking order if the WM thinks it's a good idea . We put all
 NET::KeepAbove above all Docks too, even though the spec suggests putting
 them in the same layer.

 Most transients are in the same layer as their mainwindow,
 see Workspace::constrainedStackingOrder(), they may also be in higher layers, but
 they should never be below their mainwindow.

 When some client attribute changes (above/below flag, transiency...),
 Workspace::updateClientLayer() should be called in order to make
 sure it's moved to the appropriate layer ClientList if needed.

 Currently the things that affect client in which layer a client
 belongs: KeepAbove/Keep Below flags, window type, fullscreen
 state and whether the client is active, mainclient (transiency).

 Make sure updateStackingOrder() is called in order to make
 Workspace::stackingOrder() up to date and propagated to the world.
 Using Workspace::blockStackingUpdates() (or the StackingUpdatesBlocker
 helper class) it's possible to temporarily disable updates
 and the stacking order will be updated once after it's allowed again.

 SELI TODO
- FullScreen support is not yet complete (even though the layering for it
     should be ok) - also NET::Override way of creating fullscreens needs
     to be checked
*/

#include <assert.h>

#include "utils.h"
#include "client.h"
#include "workspace.h"
#include "tabbox.h"
#include "popupinfo.h"
#include "group.h"
#include <kdebug.h>

namespace KWinInternal
{

//*******************************
// Workspace
//*******************************

void Workspace::updateClientLayer( Client* c )
    {
    if( c == NULL )
        return;
    if( c->layer() == c->belongsToLayer())
        return;
    StackingUpdatesBlocker blocker( this );
    c->invalidateLayer(); // invalidate, will be updated when doing restacking
    for( ClientList::ConstIterator it = c->transients().begin();
         it != c->transients().end();
         ++it )
        updateClientLayer( *it );
    }

void Workspace::updateStackingOrder( bool propagate_new_clients )
    {
    if( block_stacking_updates > 0 )
        {
        blocked_propagating_new_clients |= propagate_new_clients;
        return;
        }
    ClientList new_stacking_order = constrainedStackingOrder();
    bool changed = ( new_stacking_order != stacking_order );
    stacking_order = new_stacking_order;
#if 0
    kdDebug() << "stacking:" << changed << endl;
    if( changed || propagate_new_clients )
        {
        for( ClientList::ConstIterator it = stacking_order.begin();
             it != stacking_order.end();
             ++it )
            kdDebug() << (void*)(*it) << *it << endl;
        }
#endif
    if( changed || propagate_new_clients )
        propagateClients( propagate_new_clients );
    }

/*!
  Propagates the managed clients to the world.
  Called ONLY from updateStackingOrder().
 */
void Workspace::propagateClients( bool propagate_new_clients )
    {
    Window *cl; // MW we should not assume WId and Window to be compatible
                                // when passig pointers around.

    // restack the windows according to the stacking order
    Window* new_stack = new Window[ stacking_order.count() + 2 ];
    int pos = 0;
    // Stack all windows under the support window. The support window is
    // not used for anything (besides the NETWM property), and it's not shown,
    // but it was lowered after kwin startup. Stacking all clients below
    // it ensures that no client will be ever shown above override-redirect
    // windows (e.g. popups).
    new_stack[ pos++ ] = supportWindow->winId();
    int topmenu_space_pos = 0;
    for( ClientList::ConstIterator it = stacking_order.fromLast();
         it != stacking_order.end();
         --it )
        {
        new_stack[ pos++ ] = (*it)->frameId();
        if( (*it)->isTopMenu())
            topmenu_space_pos = pos;
        }
    if( topmenu_space != NULL )
        { // make sure the topmenu space is below all topmenus, if there are any
        for( int i = pos;
             i > topmenu_space_pos;
             --i )
            new_stack[ i ] = new_stack[ i - 1 ];
        new_stack[ topmenu_space_pos ] = topmenu_space->winId();
        ++pos;
        }
    // TODO isn't it too inefficient to restart always all clients?
    // TODO don't restack not visible windows?
    XRestackWindows(qt_xdisplay(), new_stack, pos);
    delete [] new_stack;

    if ( propagate_new_clients )
        {
        cl = new Window[ desktops.count() + clients.count()];
        pos = 0;
	// TODO this is still not completely in the map order
        for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it )
            cl[pos++] =  (*it)->window();
        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it )
            cl[pos++] =  (*it)->window();
        rootInfo->setClientList( cl, pos );
        delete [] cl;
        }

    cl = new Window[ stacking_order.count()];
    pos = 0;
    for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it)
        cl[pos++] =  (*it)->window();
    rootInfo->setClientListStacking( cl, pos );
    delete [] cl;

#if 0 // not necessary anymore?
    if ( tab_box->isVisible() )
        tab_box->raise();

    if ( popupinfo->isVisible() )
        popupinfo->raise();

    raiseElectricBorders();
#endif
    }


/*!
  Returns topmost visible client. Windows on the dock, the desktop
  or of any other special kind are excluded. Also if the window
  doesn't accept focus it's excluded.
 */
// TODO misleading name for this method
Client* Workspace::topClientOnDesktop( int desktop ) const
    {
// TODO    Q_ASSERT( block_stacking_updates == 0 );
    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) 
        {
        if ( (*it)->isOnDesktop( desktop ) && !(*it)->isSpecialWindow()
            && (*it)->isShown( false ) && (*it)->wantsTabFocus())
            return *it;
        }
    return 0;
    }

Client* Workspace::findDesktop( bool topmost, int desktop ) const
    {
// TODO    Q_ASSERT( block_stacking_updates == 0 );
    if( topmost )
        {
        for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it)
            {
            if ( (*it)->isOnDesktop( desktop ) && (*it)->isDesktop()
                && (*it)->isShown( true ))
                return *it;
            }
        }
    else // bottom-most
        {
        for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it)
            {
            if ( (*it)->isOnDesktop( desktop ) && (*it)->isDesktop()
                && (*it)->isShown( true ))
                return *it;
            }
        }
    return NULL;
    }

void Workspace::raiseOrLowerClient( Client *c)
    {
    if (!c) return;
    Client* topmost = NULL;
// TODO    Q_ASSERT( block_stacking_updates == 0 );
    if ( most_recently_raised && stacking_order.contains( most_recently_raised ) &&
         most_recently_raised->isShown( true ) && c->isOnCurrentDesktop())
        topmost = most_recently_raised;
    else
        topmost = topClientOnDesktop( c->isOnAllDesktops() ? currentDesktop() : c->desktop());

    if( c == topmost)
        lowerClient(c);
    else
        raiseClient(c);
    }


void Workspace::lowerClient( Client* c )
    {
    if ( !c )
        return;

    StackingUpdatesBlocker blocker( this );

    unconstrained_stacking_order.remove( c );
    unconstrained_stacking_order.prepend( c );
    if( c->isTransient())
        {
        // lower also mainclients, in their reversed stacking order
        ClientList mainclients = ensureStackingOrder( c->mainClients());
        for( ClientList::ConstIterator it = mainclients.fromLast();
             it != mainclients.end();
             ++it )
            lowerClient( *it );
        }

    if ( c == most_recently_raised )
        most_recently_raised = 0;
    }

void Workspace::lowerClientWithinApplication( Client* c )
    {
    if ( !c )
        return;

    StackingUpdatesBlocker blocker( this );

    unconstrained_stacking_order.remove( c );
    bool lowered = false;
    // first try to put it below the bottom-most window of the application
    for( ClientList::Iterator it = unconstrained_stacking_order.begin();
         it != unconstrained_stacking_order.end();
         ++it )
        if( Client::belongToSameApplication( *it, c ))
            {
            unconstrained_stacking_order.insert( it, c );
            lowered = true;
            break;
            }
    if( !lowered )
        unconstrained_stacking_order.prepend( c );
    // ignore mainwindows
    }

void Workspace::raiseClient( Client* c )
    {
    if ( !c )
        return;

    StackingUpdatesBlocker blocker( this );

    if( c->isTransient())
        {
        ClientList mainclients = ensureStackingOrder( c->mainClients());
        for( ClientList::ConstIterator it = mainclients.begin();
             it != mainclients.end();
             ++it )
            raiseClient( *it );
        }

    unconstrained_stacking_order.remove( c );
    unconstrained_stacking_order.append( c );

    if( !c->isSpecialWindow())
        most_recently_raised = c;
    }

void Workspace::raiseClientWithinApplication( Client* c )
    {
    if ( !c )
        return;

    StackingUpdatesBlocker blocker( this );
    // ignore mainwindows
    
    unconstrained_stacking_order.remove( c );
    bool raised = false;
    // first try to put it above the top-most window of the application
    for( ClientList::Iterator it = unconstrained_stacking_order.fromLast();
         it != unconstrained_stacking_order.end();
         --it )
        if( Client::belongToSameApplication( *it, c ))
            {
            ++it; // insert after the found one
            unconstrained_stacking_order.insert( it, c );
            raised = true;
            break;
            }
    if( !raised )
        restackClientUnderActive( c );
    }

void Workspace::raiseClientRequest( Client* c )
    {
    if( allowFullClientRaising( c ))
        raiseClient( c );
    else
        {
        raiseClientWithinApplication( c );
        c->demandAttention();
        }
    }

void Workspace::lowerClientRequest( Client* c )
    {
    // If the client has support for all this focus stealing prevention stuff,
    // do only lowering within the application, as that's the more logical
    // variant of lowering when application requests it.
    // No demanding of attention here of course.
    if( c->hasUserTimeSupport())
        lowerClientWithinApplication( c );
    else
        lowerClient( c );
    }

void Workspace::restackClientUnderActive( Client* c )
    {
    if( !active_client || active_client == c )
        {
        raiseClient( c );
        return;
        }

    // put in the stacking order below _all_ windows belonging to the active application
    assert( unconstrained_stacking_order.contains( active_client ));
    for( ClientList::Iterator it = unconstrained_stacking_order.fromLast();
         it != unconstrained_stacking_order.end();
         --it )
        {
        if( Client::belongToSameApplication( active_client, *it ))
            {
            unconstrained_stacking_order.remove( c );
            unconstrained_stacking_order.insert( it, c );
            break;
            }
        }
    assert( unconstrained_stacking_order.contains( c ));
    if( c->wantsTabFocus() && focus_chain.contains( active_client ))
        {
        // also put in focus_chain after all windows belonging to the active application
        focus_chain.remove( c );
        for( ClientList::Iterator it = focus_chain.fromLast();
             it != focus_chain.end();
             --it )
            {
            if( Client::belongToSameApplication( active_client, *it ))
                {
                focus_chain.insert( it, c );
                break;
                }
            }
        }
    updateStackingOrder();
    }

void Workspace::circulateDesktopApplications()
    {
    if ( desktops.count() > 1 )
        {
        bool change_active = activeClient()->isDesktop();
        raiseClient( findDesktop( false, currentDesktop()));
        if( change_active ) // if the previously topmost Desktop was active, activate this new one
            activateClient( findDesktop( true, currentDesktop()));
        }
    // if there's no active client, make desktop the active one
    if( desktops.count() > 0 && activeClient() == NULL && should_get_focus.count() == 0 )
        activateClient( findDesktop( true, currentDesktop()));
    }


/*!
  Returns a stacking order based upon \a list that fulfills certain contained.
 */
ClientList Workspace::constrainedStackingOrder()
    {
    ClientList layer[ NumLayers ];

#if 0
    kdDebug() << "stacking1:" << endl;
#endif
    // build the order from layers
    for( ClientList::ConstIterator it = unconstrained_stacking_order.begin();
         it != unconstrained_stacking_order.end();
         ++it )
        {
#if 0
        kdDebug() << (void*)(*it) << *it << endl;
#endif
        layer[ (*it)->layer() ].append( *it );
        }
    ClientList stacking;    
    for( Layer lay = FirstLayer;
         lay < NumLayers;
         ++lay )    
        stacking += layer[ lay ];
#if 0
    kdDebug() << "stacking2:" << endl;
    for( ClientList::ConstIterator it = stacking.begin();
         it != stacking.end();
         ++it )
        kdDebug() << (void*)(*it) << *it << endl;
#endif
    // now keep transients above their mainwindows
    // TODO this could(?) use some optimization
    for( ClientList::Iterator it = stacking.fromLast();
         it != stacking.end();
         )
        {
        if( !(*it)->isTransient())
            {
            --it;
            continue;
            }
        ClientList::Iterator it2 = stacking.end();
        if( (*it)->groupTransient())
            {
            if( (*it)->group()->members().count() > 0 )
                { // find topmost client this one is transient for
                for( it2 = stacking.fromLast();
                     it2 != stacking.end();
                     --it2 )
                    {
                    if( *it2 == *it )
                        {
                        it2 = stacking.end(); // don't reorder
                        break;
                        }
                    if( (*it2)->hasTransient( *it, true ) && keepTransientAbove( *it2, *it ))
                        break;
                    }
                } // else it2 remains pointing at stacking.end()
            }
        else
            {
            for( it2 = stacking.fromLast();
                 it2 != stacking.end();
                 --it2 )
                {
                if( *it2 == *it )
                    {
                    it2 = stacking.end(); // don't reorder
                    break;
                    }
                if( *it2 == (*it)->transientFor() && keepTransientAbove( *it2, *it ))
                    break;
                }
            }
//        kdDebug() << "STACK:" << (*it) << ":" << ( it2 == stacking.end() ? ((Client*)0) : (*it2)) << endl;
        if( it2 == stacking.end())
            {
            --it;
            continue;
            }
        Client* current = *it;
        ClientList::Iterator remove_it = it;
        --it;
        stacking.remove( remove_it );
        if( !current->transients().isEmpty())  // this one now can be possibly above its transients,
            it = it2; // so go again higher in the stack order and possibly move those transients again
        ++it2; // insert after the mainwindow, it's ok if it2 is now stacking.end()
        stacking.insert( it2, current );
        }
#if 0
    kdDebug() << "stacking3:" << endl;
    for( ClientList::ConstIterator it = stacking.begin();
         it != stacking.end();
         ++it )
        kdDebug() << (void*)(*it) << *it << endl;
    kdDebug() << "\n\n" << endl;
#endif
    return stacking;
    }

void Workspace::blockStackingUpdates( bool block )
    {
    if( block )
        {
        if( block_stacking_updates == 0 )
            blocked_propagating_new_clients = false;
        ++block_stacking_updates;
        }
    else // !block
        if( --block_stacking_updates == 0 )
            updateStackingOrder( blocked_propagating_new_clients );
    }

// Ensure list is in stacking order
ClientList Workspace::ensureStackingOrder( const ClientList& list ) const
    {
// TODO    Q_ASSERT( block_stacking_updates == 0 );
    if( list.count() < 2 )
        return list;
    // TODO is this worth optimizing?
    ClientList result = list;
    for( ClientList::ConstIterator it = stacking_order.begin();
         it != stacking_order.end();
         ++it )
        if( result.remove( *it ) != 0 )
            result.append( *it );
    return result;
    }

// check whether a transient should be actually kept above its mainwindow
// there may be some special cases where this rule shouldn't be enfored
bool Workspace::keepTransientAbove( const Client* mainwindow, const Client* transient )
    {
    return true;
    // #63223 - don't keep transients above docks, because the dock is kept high,
    // and e.g. dialogs for them would be too high too
    // TODO this doesn't really work - the transient should be raised after clicking
    // on the dock, but docks don't become active after clicking them
    if( mainwindow->isDock() && !mainwindow->keepBelow()
        && !mainwindow->isActive() && !transient->isActive()) // TODO !w->group()->isActive() ???
        return false;
    return true;
    }

//*******************************
// Client
//*******************************

void Client::restackWindow( Window /*above TODO */, int detail, NET::RequestSource source, bool send_event )
    {
    switch ( detail )
        {
        case Above:
        case TopIf:
            if( source == NET::FromTool )
                workspace()->raiseClient( this );
            else
                workspace()->raiseClientRequest( this );
            break;
        case Below:
        case BottomIf:
            if( source == NET::FromTool )
                workspace()->lowerClient( this );
            else
                workspace()->lowerClientRequest( this );
            break;
        case Opposite:
        default:
            break;
        }
    if( send_event )
        sendSyntheticConfigureNotify();
    }
    
void Client::setKeepAbove( bool b )
    {
    if ( b == keepAbove() )
        return;
    setKeepBelow( false );
    keep_above = b;
    info->setState( b ? NET::KeepAbove : 0, NET::KeepAbove );
    // TODO emit a signal about the change to the style plugin
    workspace()->updateClientLayer( this );
    }

void Client::setKeepBelow( bool b )
    {
    if ( b == keepBelow() )
        return;
    setKeepAbove( false );
    keep_below = b;
    info->setState( b ? NET::KeepBelow : 0, NET::KeepBelow );
    workspace()->updateClientLayer( this );
    }

Layer Client::layer() const
    {
    if( in_layer == UnknownLayer )
        const_cast< Client* >( this )->in_layer = belongsToLayer();
    return in_layer;
    }

Layer Client::belongsToLayer() const
    {
    if( isDesktop())
        return DesktopLayer;
    if( isSplash())         // no damn annoying splashscreens
        return NormalLayer; // getting in the way of everything else
    if( isDock() && keepBelow())
        // slight hack for the 'allow window to cover panel' Kicker setting
        // don't move keepbelow docks below normal window, but only to the same
        // layer, so that both may be raised to cover the other
        return NormalLayer;
    if( keepBelow())
        return BelowLayer;
    if( isDock() && !keepBelow())
        return DockLayer;
    if( isTopMenu())
        return DockLayer;
    if( isDialog() && workspace()->activeClient() == this
        && ( options->focusPolicy == Options::ClickToFocus || options->autoRaise ))
        return ActiveLayer;
    if( keepAbove())
        return AboveLayer;
    if( isFullScreen() && workspace()->activeClient() != NULL
        && ( workspace()->activeClient() == this || this->hasTransient( workspace()->activeClient(), true )))
        return ActiveLayer;
    return NormalLayer;
    }

} // namespace
