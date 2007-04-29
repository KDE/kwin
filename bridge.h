/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_BRIDGE_H
#define KWIN_BRIDGE_H

#include <kdecoration_p.h>

namespace KWin
{

class Client;

class Bridge : public KDecorationBridge
    {
    public:
        Bridge( Client* cl );
        virtual bool isActive() const;
        virtual bool isCloseable() const;
        virtual bool isMaximizable() const;
        virtual MaximizeMode maximizeMode() const;
        virtual bool isMinimizable() const;
        virtual bool providesContextHelp() const;
        virtual int desktop() const;
        virtual bool isModal() const;
        virtual bool isShadeable() const;
        virtual bool isShade() const;
        virtual bool isSetShade() const;
        virtual bool keepAbove() const;
        virtual bool keepBelow() const;
        virtual bool isMovable() const;
        virtual bool isResizable() const;
        virtual NET::WindowType windowType( unsigned long supported_types ) const;
        virtual QIcon icon() const;
        virtual QString caption() const;
        virtual void processMousePressEvent( QMouseEvent* );
        virtual void showWindowMenu( QPoint );
        virtual void showWindowMenu( const QRect & );
        virtual void performWindowOperation( WindowOperation );
        virtual void setMask( const QRegion&, int );
        virtual bool isPreview() const;
        virtual QRect geometry() const;
        virtual QRect iconGeometry() const;
        virtual QRegion unobscuredRegion( const QRegion& r ) const;
        virtual QWidget* workspaceWidget() const;
        virtual WId windowId() const;
        virtual void closeWindow();
        virtual void maximize( MaximizeMode mode );
        virtual void minimize();
        virtual void showContextHelp();
        virtual void setDesktop( int desktop );
        virtual void titlebarDblClickOperation();
        virtual void titlebarMouseWheelOperation( int delta );
        virtual void setShade( bool set );
        virtual void setKeepAbove( bool );
        virtual void setKeepBelow( bool );
        virtual int currentDesktop() const;
        virtual QWidget* initialParentWidget() const;
        virtual Qt::WFlags initialWFlags() const;
        virtual void helperShowHide( bool show );
        virtual void grabXServer( bool grab );
    private:
        Client* c;
    };

} // namespace

#endif
