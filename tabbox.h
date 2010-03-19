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

#ifndef KWIN_TABBOX_H
#define KWIN_TABBOX_H

#include <QTimer>
#include <QModelIndex>
#include "utils.h"
#include "tabbox/tabboxhandler.h"

class QKeyEvent;

namespace KWin
{

class Workspace;
class Client;
namespace TabBox
{
class TabBoxConfig;

class TabBoxHandlerImpl : public TabBoxHandler
    {
    public:
        TabBoxHandlerImpl();
        virtual ~TabBoxHandlerImpl();

        virtual int activeScreen() const;
        virtual TabBoxClient* activeClient() const;
        virtual int currentDesktop() const;
        virtual QString desktopName( TabBoxClient* client ) const;
        virtual QString desktopName( int desktop ) const;
        virtual TabBoxClient* nextClientFocusChain( TabBoxClient* client ) const;
        virtual int nextDesktopFocusChain( int desktop ) const;
        virtual int numberOfDesktops() const;
        virtual TabBoxClientList stackingOrder() const;
        virtual TabBoxClient* clientToAddToList( TabBoxClient* client, int desktop, bool allDesktops ) const;
        virtual TabBoxClient* desktopClient() const;
    };

class TabBoxClientImpl : public TabBoxClient
    {
    public:
        TabBoxClientImpl();
        virtual ~TabBoxClientImpl();

        virtual QString caption() const;
        virtual QPixmap icon() const;
        virtual WId window() const;
        virtual bool isMinimized() const;
        virtual int x() const;
        virtual int y() const;
        virtual int width() const;
        virtual int height() const;

        Client* client() const { return m_client; }
        void setClient( Client* client ) { m_client = client; }

    private:
        Client* m_client;
    };

class TabBox : public QObject
    {
    Q_OBJECT
    public:
        TabBox( Workspace *ws );
        ~TabBox();

        Client* currentClient();
        ClientList currentClientList();
        int currentDesktop();
        QList< int > currentDesktopList();

        void setCurrentClient( Client* newClient );
        void setCurrentDesktop( int newDesktop );

        void setMode( TabBoxMode mode );
        TabBoxMode mode() const { return m_tabBoxMode; }

        void reset( bool partial_reset = false );
        void nextPrev( bool next = true);

        void delayedShow();
        void hide();

        void refDisplay();
        void unrefDisplay();
        bool isDisplayed() const;

        void handleMouseEvent( XEvent* );
        void grabbedKeyEvent( QKeyEvent* event );

        Workspace* workspace() const;

        void reconfigure();

    public slots:
        void show();

    private:
        void setCurrentIndex( QModelIndex index, bool notifyEffects = true );
        void loadConfig( const KConfigGroup& config, TabBoxConfig& tabBoxConfig );

    private:
        Workspace* wspace;
        TabBoxMode m_tabBoxMode;
        QModelIndex m_index;
        TabBoxHandlerImpl* m_tabBox;
        bool m_delayShow;
        int m_delayShowTime;

        QTimer delayedShowTimer;
        int display_refcount;

        TabBoxConfig m_defaultConfig;
        TabBoxConfig m_alternativeConfig;
        TabBoxConfig m_desktopConfig;
        TabBoxConfig m_desktopListConfig;
        // false if an effect has referenced the tabbox
        // true if tabbox is active (independent on showTabbox setting)
        bool m_isShown;
    };

/*!
  Returns the tab box' workspace
 */
inline Workspace* TabBox::workspace() const
    {
    return wspace;
    }

/*!
  Increase the reference count, preventing the default tabbox from showing.

  \sa unrefDisplay(), isDisplayed()
 */
inline void TabBox::refDisplay()
    {
    ++display_refcount;
    }

/*!
  Returns whether the tab box is being displayed, either natively or by an
  effect.

  \sa refDisplay(), unrefDisplay()
 */
inline bool TabBox::isDisplayed() const
    {
    return display_refcount > 0;
    }

} // namespace TabBox

} // namespace

#endif
