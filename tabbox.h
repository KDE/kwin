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

#include <Q3Frame>
#include <QTimer>
#include "utils.h"

class QLabel;

namespace KWinInternal
{

class Workspace;
class Client;

class TabBox : public Q3Frame
    {
    Q_OBJECT
    public:
        TabBox( Workspace *ws, const char *name=0 );
        ~TabBox();

        Client* currentClient();
        void setCurrentClient( Client* c );
        int currentDesktop();

    // DesktopMode and WindowsMode are based on the order in which the desktop
    //  or window were viewed.
    // DesktopListMode lists them in the order created.
        enum Mode { DesktopMode, DesktopListMode, WindowsMode };
        void setMode( Mode mode );
        Mode mode() const;

        void reset();
        void nextPrev( bool next = true);

        void delayedShow();
        void hide();

        void handleMouseEvent( XEvent* );

        Workspace* workspace() const;

        void reconfigure();

    protected:
        void showEvent( QShowEvent* );
        void hideEvent( QHideEvent* );
        void drawContents( QPainter * );

    private:
        void createClientList(ClientList &list, int desktop /*-1 = all*/, Client *start, bool chain);
        void updateOutline();

    private:
        Client* current_client;
        Mode m;
        Workspace* wspace;
        ClientList clients;
        int desk;
        int lineHeight;
        bool showMiniIcon;
        QTimer delayedShowTimer;
        QString no_tasks;
        bool options_traverse_all;
        Window outline_left, outline_right, outline_top, outline_bottom;
    };


/*!
  Returns the tab box' workspace
 */
inline Workspace* TabBox::workspace() const
    {
    return wspace;
    }

/*!
  Returns the current mode, either DesktopListMode or WindowsMode

  \sa setMode()
 */
inline TabBox::Mode TabBox::mode() const
    {
    return m;
    }

} // namespace

#endif
