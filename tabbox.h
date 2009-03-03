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
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <KDE/Plasma/FrameSvg>
#include "utils.h"


namespace KWin
{

class Workspace;
class Client;

class TabBox : public QGraphicsView
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

        enum SortOrder { StaticOrder, MostRecentlyUsedOrder };
        void setMode( TabBoxMode mode );
        TabBoxMode mode() const;

        void reset( bool partial_reset = false );
        void initScene();
        void nextPrev( bool next = true);

        void delayedShow();
        void hide();

        void refDisplay();
        void unrefDisplay();
        bool isDisplayed() const;

        void handleMouseEvent( XEvent* );

        Workspace* workspace() const;

        void reconfigure();
        void createClientList(ClientList &list, int desktop /*-1 = all*/, Client *start, bool chain);

        Plasma::FrameSvg* itemFrame();

    public slots:
        void show();

    protected:
        void showEvent( QShowEvent* );
        void hideEvent( QHideEvent* );
        virtual void drawBackground( QPainter * painter, const QRectF & rect );

    private:
        void createDesktopList(QList< int > &list, int start, SortOrder order);

    private:
        Workspace* wspace;
        TabBoxMode m;
        ClientList clients;
        Client* client;
        QList< int > desktops;
        int desk;

        QTimer delayedShowTimer;
        int display_refcount;
        QString no_tasks;
        int lineHeight;
        bool showMiniIcon;
        bool options_traverse_all;

        QGraphicsScene* scene;
        Plasma::FrameSvg frame;
        Plasma::FrameSvg item_frame;
    };

class TabBoxWindowItem : public QGraphicsItem
    {
    public:
        TabBoxWindowItem( Client* client, TabBox* parent );
        ~TabBoxWindowItem();
        virtual QRectF boundingRect() const;
        virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget );
        void setWidth( int width );
        void setHeight( int height );
        void setShowMiniIcons( bool showMiniIcons );
    protected:
        virtual void drawBackground( QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* );

    private:
        Client* m_client;
        TabBox* m_parent;
        int m_width;
        int m_height;
        bool m_showMiniIcons;
    };

class TabBoxDesktopItem : public QGraphicsItem
    {
    public:
        TabBoxDesktopItem( int desktop, TabBox* parent );
        ~TabBoxDesktopItem();
        virtual QRectF boundingRect() const;
        virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget );
        void setWidth( int width );
        void setHeight( int height );
    protected:
        virtual void drawBackground( QPainter* painter, const QStyleOptionGraphicsItem*, QWidget* );
    private:
        int m_desktop;
        TabBox* m_parent;
        int m_width;
        int m_height;
    };


/*!
  Returns the tab box' workspace
 */
inline Workspace* TabBox::workspace() const
    {
    return wspace;
    }

/*!
  Returns the current mode, either TabBoxDesktopListMode or TabBoxWindowsMode

  \sa setMode()
 */
inline TabBoxMode TabBox::mode() const
    {
    return m;
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

inline Plasma::FrameSvg* TabBox::itemFrame()
    {
    return &item_frame;
    }


} // namespace

#endif
