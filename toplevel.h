/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_TOPLEVEL_H
#define KWIN_TOPLEVEL_H

#include <config-X11.h>

#include <assert.h>
#include <QObject>
#include <kdecoration.h>
#include <kdebug.h>

#include "utils.h"
#include "workspace.h"

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

class NETWinInfo;

namespace KWin
{

class Workspace;
class EffectWindowImpl;

class Toplevel
    : public QObject, public KDecorationDefines
    {
    Q_OBJECT
    public:
        Toplevel( Workspace *ws );
        Window frameId() const;
        Window window() const;
        Workspace* workspace() const;
        QRect geometry() const;
        QSize size() const;
        QPoint pos() const;
        QRect rect() const;
        int x() const;
        int y() const;
        int width() const;
        int height() const;
        bool isOnScreen( int screen ) const; // true if it's at least partially there
        int screen() const; // the screen where the center is
        virtual QPoint clientPos() const = 0; // inside of geometry()
        virtual QSize clientSize() const = 0;

        // prefer isXXX() instead
        // 0 for supported types means default for managed/unmanaged types
        NET::WindowType windowType( bool direct = false, int supported_types = 0 ) const;
        bool hasNETSupport() const;
        bool isDesktop() const;
        bool isDock() const;
        bool isToolbar() const;
        bool isTopMenu() const;
        bool isMenu() const;
        bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
        bool isDialog() const;
        bool isSplash() const;
        bool isUtility() const;
        bool isDropdownMenu() const;
        bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
        bool isTooltip() const;
        bool isNotification() const;
        bool isComboBox() const;
        bool isDNDIcon() const;

        virtual int desktop() const = 0;
        bool isOnDesktop( int d ) const;
        bool isOnCurrentDesktop() const;
        bool isOnAllDesktops() const;

        QByteArray windowRole() const;
        QByteArray sessionId();
        QByteArray resourceName() const;
        QByteArray resourceClass() const;
        QByteArray wmCommand();
        QByteArray wmClientMachine( bool use_localhost ) const;
        Window wmClientLeader() const;
        pid_t pid() const;
        static bool resourceMatch( const Toplevel* c1, const Toplevel* c2 );

        Pixmap windowPixmap( bool allow_create = true ); // may return None (e.g. at a bad moment while resizing)
        bool readyForPainting() const; // true if the window has been already painted its contents
        Visual* visual() const;
        bool shape() const;
        void setOpacity( double opacity );
        double opacity() const;
        int depth() const;
        bool hasAlpha() const;
        void setupCompositing();
        void finishCompositing();
        void addRepaint( const QRect& r );
        void addRepaint( int x, int y, int w, int h );
        void addRepaintFull();
        // these call workspace->addRepaint(), but first transform the damage if needed
        void addWorkspaceRepaint( const QRect& r );
        void addWorkspaceRepaint( int x, int y, int w, int h );
        QRegion repaints() const;
        void resetRepaints( const QRect& r );
        QRegion damage() const;
        void resetDamage( const QRect& r );
        EffectWindowImpl* effectWindow();

    protected:
        virtual ~Toplevel();
        void setWindowHandles( Window client, Window frame );
        void detectShape( Window id );
        virtual void propertyNotifyEvent( XPropertyEvent* e );
#ifdef HAVE_XDAMAGE
        virtual void damageNotifyEvent( XDamageNotifyEvent* e );
#endif
        Pixmap createWindowPixmap();
        void discardWindowPixmap();
        void addDamage( const QRect& r );
        void addDamage( int x, int y, int w, int h );
        void addDamageFull();
        void getWmClientLeader();
        void getWmClientMachine();
        void getResourceClass();
        void getWindowRole();
        virtual void debug( kdbgstream& stream ) const = 0;
        void copyToDeleted( Toplevel* c );
        void disownDataPassedToDeleted();
        friend kdbgstream& operator<<( kdbgstream& stream, const Toplevel* );
        void deleteEffectWindow();
        QRect geom;
        Visual* vis;
        int bit_depth;
        NETWinInfo* info;
        bool ready_for_painting;
    private:
        static QByteArray staticWindowRole(WId);
        static QByteArray staticSessionId(WId);
        static QByteArray staticWmCommand(WId);
        static QByteArray staticWmClientMachine(WId);
        static Window staticWmClientLeader(WId);
        // when adding new data members, check also copyToDeleted()
        Window client;
        Window frame;
        Workspace* wspace;
        Pixmap window_pix;
#ifdef HAVE_XDAMAGE
        Damage damage_handle;
#endif
        QRegion damage_region; // damage is really damaged window (XDamage) and texture needs
        QRegion repaints_region; // updating, repaint just requires repaint of that area
        bool is_shape;
        EffectWindowImpl* effect_window;
        QByteArray resource_name;
        QByteArray resource_class;
        QByteArray client_machine;
        WId wmClientLeaderWin;
        QByteArray window_role;
        // when adding new data members, check also copyToDeleted()
    };

inline Window Toplevel::window() const
    {
    return client;
    }

inline Window Toplevel::frameId() const
    {
    return frame;
    }

inline void Toplevel::setWindowHandles( Window w, Window f )
    {
    assert( client == None && w != None );
    client = w;
    assert( frame == None && f != None );
    frame = f;
    }

inline Workspace* Toplevel::workspace() const
    {
    return wspace;
    }

inline QRect Toplevel::geometry() const
    {
    return geom;
    }

inline QSize Toplevel::size() const
    {
    return geom.size();
    }

inline QPoint Toplevel::pos() const
    {
    return geom.topLeft();
    }

inline int Toplevel::x() const
    {
    return geom.x();
    }

inline int Toplevel::y() const
    {
    return geom.y();
    }

inline int Toplevel::width() const
    {
    return geom.width();
    }

inline int Toplevel::height() const
    {
    return geom.height();
    }

inline QRect Toplevel::rect() const
    {
    return QRect( 0, 0, width(), height());
    }

inline bool Toplevel::readyForPainting() const
    {
    return ready_for_painting;
    }

inline Visual* Toplevel::visual() const
    {
    return vis;
    }

inline bool Toplevel::isDesktop() const
    {
    return windowType() == NET::Desktop;
    }

inline bool Toplevel::isDock() const
    {
    return windowType() == NET::Dock;
    }

inline bool Toplevel::isTopMenu() const
    {
    return windowType() == NET::TopMenu;
    }

inline bool Toplevel::isMenu() const
    {
    return windowType() == NET::Menu && !isTopMenu(); // because of backwards comp.
    }

inline bool Toplevel::isToolbar() const
    {
    return windowType() == NET::Toolbar;
    }

inline bool Toplevel::isSplash() const
    {
    return windowType() == NET::Splash;
    }

inline bool Toplevel::isUtility() const
    {
    return windowType() == NET::Utility;
    }

inline bool Toplevel::isDialog() const
    {
    return windowType() == NET::Dialog;
    }

inline bool Toplevel::isNormalWindow() const
    {
    return windowType() == NET::Normal;
    }

inline bool Toplevel::isDropdownMenu() const
    {
    return windowType() == NET::DropdownMenu;
    }

inline bool Toplevel::isPopupMenu() const
    {
    return windowType() == NET::PopupMenu;
    }

inline bool Toplevel::isTooltip() const
    {
    return windowType() == NET::Tooltip;
    }

inline bool Toplevel::isNotification() const
    {
    return windowType() == NET::Notification;
    }

inline bool Toplevel::isComboBox() const
    {
    return windowType() == NET::ComboBox;
    }

inline bool Toplevel::isDNDIcon() const
    {
    return windowType() == NET::DNDIcon;
    }

inline Pixmap Toplevel::windowPixmap( bool allow_create )
    {
    if( window_pix == None && allow_create )
        window_pix = createWindowPixmap();
    return window_pix;
    }

inline QRegion Toplevel::damage() const
    {
    return damage_region;
    }

inline QRegion Toplevel::repaints() const
    {
    return repaints_region;
    }

inline bool Toplevel::shape() const
    {
    return is_shape;
    }

inline int Toplevel::depth() const
    {
    return bit_depth;
    }

inline bool Toplevel::hasAlpha() const
    {
    return depth() == 32;
    }

inline
EffectWindowImpl* Toplevel::effectWindow()
    {
    return effect_window;
    }

inline bool Toplevel::isOnAllDesktops() const
    {
    return desktop() == NET::OnAllDesktops;
    }

inline bool Toplevel::isOnDesktop( int d ) const
    {
    return desktop() == d || /*desk == 0 ||*/ isOnAllDesktops();
    }

inline bool Toplevel::isOnCurrentDesktop() const
    {
    return isOnDesktop( workspace()->currentDesktop());
    }

inline QByteArray Toplevel::resourceName() const
    {
    return resource_name; // it is always lowercase
    }

inline QByteArray Toplevel::resourceClass() const
    {
    return resource_class; // it is always lowercase
    }

inline QByteArray Toplevel::windowRole() const
    {
    return window_role;
    }

inline pid_t Toplevel::pid() const
    {
    return info->pid();
    }

kdbgstream& operator<<( kdbgstream& stream, const Toplevel* );
kdbgstream& operator<<( kdbgstream& stream, const ToplevelList& );
kdbgstream& operator<<( kdbgstream& stream, const ConstToplevelList& );

KWIN_COMPARE_PREDICATE( WindowMatchPredicate, Toplevel, Window, cl->window() == value );
KWIN_COMPARE_PREDICATE( FrameIdMatchPredicate, Toplevel, Window, cl->frameId() == value );

} // namespace

#endif
