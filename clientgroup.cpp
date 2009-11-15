/*******************************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#include "clientgroup.h"

#include "client.h"
#include "effects.h"

namespace KWin
{

ClientGroup::ClientGroup( Client *c )
    : clients_()
    , items_()
    , visible_( 0 )
    , minSize_( 0, 0 )
    , maxSize_( INT_MAX, INT_MAX )
    {
    clients_.append( c );
    updateItems();
    updateMinMaxSize();
    Workspace::self()->addClientGroup( this );
    c->setClientShown( true ); // Ensure the client is visible
    c->triggerDecorationRepaint(); // TODO: Required? Maybe for creating new group?
    }

ClientGroup::~ClientGroup()
    {
    Workspace::self()->removeClientGroup( this );
    }

void ClientGroup::add( Client* c, int before, bool becomeVisible )
    {
    if( contains( c ) || !c->workspace()->decorationSupportsClientGrouping() )
        return;

    // Client must not already be grouped
    assert( c->clientGroup()->clients().size() == 1 );

    // Tabbed windows MUST have a decoration
    if( c->noBorder() )
        c->setNoBorder( false );
    if( clients_[visible_]->noBorder() )
        clients_[visible_]->setNoBorder( false );

    // Notify effects of merge
    if( effects != NULL )
        static_cast<EffectsHandlerImpl*>(effects)->clientGroupItemAdded(
            c->effectWindow(), clients_[visible_]->effectWindow() );

    // Delete old group and update
    delete c->clientGroup(); // Delete old group as it's now empty
    c->setClientGroup( this ); // Let the client know which group it belongs to

    // Actually add to new group
    if( before >= 0 )
        {
        if( visible_ >= before )
            visible_++;
        clients_.insert( before, c );
        }
    else
        clients_.append( c );
    if( !becomeVisible ) // Hide before adding
        c->setClientShown( false );
    updateItems();
    updateMinMaxSize();
    updateStates( clients_[visible_], c );

    c->setGeometry( clients_[visible_]->geometry() );
    if( becomeVisible ) // Set visible after settings geometry
        setVisible( c );

    // Activate the new visible window
    clients_[visible_]->setActive( true );
    //clients_[visible_]->takeFocus( Allowed );
    //clients_[visible_]->workspace()->raiseClient( clients_[visible_] );

    clients_[visible_]->triggerDecorationRepaint();
    }

void ClientGroup::remove( int index, const QRect& newGeom )
    {
    remove( clients_[index], newGeom );
    }

void ClientGroup::remove( Client* c, const QRect& newGeom )
    {
    if( !c )
        return;
    if( clients_.count() < 2 )
        {
        c->setClientGroup( NULL );
        Workspace::self()->removeClientGroup( this ); // Remove immediately
        delete this;
        return;
        }

    ClientList::const_iterator i;
    Client* newVisible = clients_[visible_];
    if( newVisible == c )
        newVisible = ( visible_ != clients_.size() - 1 ) ? clients_[visible_ + 1] : clients_[visible_ - 1];

    // Notify effects of removal
    if( effects )
        static_cast<EffectsHandlerImpl*>(effects)->clientGroupItemRemoved(
            c->effectWindow(), newVisible->effectWindow() );

    setVisible( newVisible ); // Display new window before removing old one
    clients_.removeAll( c );
    visible_ = indexOfClient( newVisible ); // Index may have changed
    updateItems();
    updateMinMaxSize();

    c->setClientGroup( new ClientGroup( c ));
    if( newGeom.isValid() )
        c->setGeometry( newGeom );

    newVisible->triggerDecorationRepaint();
    }

void ClientGroup::removeAll()
    {
    while( clients_.count() > 1 )
        remove( clients_.at( 1 ));
    }

void ClientGroup::closeAll()
    {
    Client* front;
    ClientList list( clients_ );
    while( !list.isEmpty() )
        {
        front = list.front();
        list.pop_front();
        if( front != clients_[visible_] )
            front->closeWindow();
        }
    clients_[visible_]->closeWindow();
    }

void ClientGroup::move( int index, int before )
    {
    move( clients_[index], ( before >= 0 && before < clients_.size() ) ? clients_[before] : NULL );
    }

void ClientGroup::move( Client* c, Client* before )
    {
    if( c == before ) // Impossible to do
        return;

    Client* wasVisible = clients_[visible_];
    clients_.removeAll( c );
    clients_.insert( before ? indexOfClient( before ) : clients_.size(), c );
    visible_ = indexOfClient( wasVisible );
    updateItems();

    clients_[visible_]->triggerDecorationRepaint();
    }

void ClientGroup::displayClientMenu( int index, const QPoint& pos )
    {
    displayClientMenu( clients_[index], pos );
    }

void ClientGroup::displayClientMenu( Client* c, const QPoint& pos )
    {
    c->workspace()->showWindowMenu( pos, c );
    }

bool ClientGroup::containsActiveClient()
    {
    return contains( Workspace::self()->activeClient() );
    }

void ClientGroup::setVisible( int index )
    {
    setVisible( clients_[index] );
    }

void ClientGroup::setVisible( Client* c )
    {
    if( c == clients_[visible_] || !contains( c ))
        return;

    // Notify effects of switch
    if( effects != NULL )
        static_cast<EffectsHandlerImpl*>(effects)->clientGroupItemSwitched(
            clients_[visible_]->effectWindow(), c->effectWindow() );

    visible_ = indexOfClient( c );
    c->setClientShown( true );
    for( ClientList::const_iterator i = clients_.begin(); i != clients_.end(); i++ )
        if( (*i) != c )
            (*i)->setClientShown( false );
    }

void ClientGroup::updateStates( Client* main, Client* only )
    {
    for( ClientList::const_iterator i = clients_.begin(); i != clients_.end(); i++ )
        if( (*i) != main && ( !only || (*i) == only ))
            {
            if( (*i)->isMinimized() != main->isMinimized() )
                {
                if( main->isMinimized() )
                    (*i)->minimize( true );
                else
                    (*i)->unminimize( true );
                }
            if( (*i)->isShade() != main->isShade() )
                (*i)->setShade( main->isShade() ? ShadeNormal : ShadeNone );
            if( (*i)->geometry() != main->geometry() )
                (*i)->setGeometry( main->geometry() );
            if( (*i)->desktop() != main->desktop() )
                (*i)->setDesktop( main->desktop() );
            if( (*i)->isOnAllDesktops() != main->isOnAllDesktops() )
                (*i)->setOnAllDesktops( main->isOnAllDesktops() );
            if( (*i)->keepAbove() != main->keepAbove() )
                (*i)->setKeepAbove( main->keepAbove() );
            if( (*i)->keepBelow() != main->keepBelow() )
                (*i)->setKeepBelow( main->keepBelow() );
            }
    }

void ClientGroup::updateItems()
    {
    items_.clear();
    for( ClientList::const_iterator i = clients_.begin(); i != clients_.end(); i++ )
        {
        QIcon icon( (*i)->icon() );
        icon.addPixmap( (*i)->miniIcon() );
        items_.append( ClientGroupItem( (*i)->caption(), icon ));
        }
    }

void ClientGroup::updateMinMaxSize()
    {
    // Determine entire group's minimum and maximum sizes
    minSize_ = QSize( 0, 0 );
    maxSize_ = QSize( INT_MAX, INT_MAX );
    for( ClientList::const_iterator i = clients_.begin(); i != clients_.end(); i++ )
        {
        if( (*i)->minSize().width() > minSize_.width() )
            minSize_.setWidth( (*i)->minSize().width() );
        if( (*i)->minSize().height() > minSize_.height() )
            minSize_.setHeight( (*i)->minSize().height() );
        if( (*i)->maxSize().width() < maxSize_.width() )
            maxSize_.setWidth( (*i)->maxSize().width() );
        if( (*i)->maxSize().height() < maxSize_.height() )
            maxSize_.setHeight( (*i)->maxSize().height() );
        }
    if( minSize_.width() > maxSize_.width() ||
        minSize_.height() > maxSize_.height() )
        {
        kWarning(1212) << "ClientGroup's min size is greater than its' max size. Setting max to min.";
        maxSize_ = minSize_;
        }

    // Ensure all windows are within these sizes
    const QSize size = clients_[visible_]->size();
    QSize newSize(
        qBound( minSize_.width(), size.width(), maxSize_.width() ),
        qBound( minSize_.height(), size.height(), maxSize_.height() ));
    if( newSize != size )
        for( ClientList::const_iterator i = clients_.begin(); i != clients_.end(); i++ )
            // TODO: Doesn't affect shaded windows?
            // There seems to be a race condition when using plainResize() which causes the window
            // to sometimes be located at new window's location instead of the visible window's location
            // when a window with a large min size is added to a group with a small window size.
            (*i)->setGeometry( QRect( clients_[visible_]->pos(), newSize )); 
    }

}
