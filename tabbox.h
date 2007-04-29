/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_TABBOX_H
#define KWIN_TABBOX_H

#include <QFrame>
#include <QTimer>
#include "utils.h"

class QLabel;

namespace KWin
{

class Workspace;
class Client;

class TabBox : public QFrame
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
        void nextPrev( bool next = true);

        void delayedShow();
        void hide();

        void refDisplay();
        void unrefDisplay();
        bool isDisplayed() const;

        void handleMouseEvent( XEvent* );

        Workspace* workspace() const;

        void reconfigure();
        void updateKeyMapping();

    public slots:
        void show();

    protected:
        void showEvent( QShowEvent* );
        void hideEvent( QHideEvent* );
        void paintEvent( QPaintEvent* );

    private:
        void createClientList(ClientList &list, int desktop /*-1 = all*/, Client *start, bool chain);
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

} // namespace

#endif
