/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef KDECORATIONBRIDGE_H
#define KDECORATIONBRIDGE_H

#include "kdecoration.h"
#include <QtGui/QWidget>

/** @addtogroup kdecoration */
/** @{ */

/**
 * @short Bridge class for communicating between decorations and KWin core.
 *
 * This class allows communication between decorations and KWin core while allowing
 * to keep binary compatibility. Decorations do not need to use it directly at all.
 */
// This class is supposed to keep binary compatibility, just like KDecoration.
// Extending should be done the same way, i.e. inheriting KDecorationBridge2 from it
// and adding new functionality there.
class KDecorationBridge : public KDecorationDefines
    {
    public:
        virtual ~KDecorationBridge(){}
        virtual bool isActive() const = 0;
        virtual bool isCloseable() const = 0;
        virtual bool isMaximizable() const = 0;
        virtual MaximizeMode maximizeMode() const = 0;
        virtual bool isMinimizable() const = 0;
        virtual bool providesContextHelp() const = 0;
        virtual int desktop() const = 0;
        virtual bool isModal() const = 0;
        virtual bool isShadeable() const = 0;
        virtual bool isShade() const = 0;
        virtual bool isSetShade() const = 0;
        virtual bool keepAbove() const = 0;
        virtual bool keepBelow() const = 0;
        virtual bool isMovable() const = 0;
        virtual bool isResizable() const = 0;
        virtual NET::WindowType windowType( unsigned long supported_types ) const = 0;
        virtual QIcon icon() const = 0;
        virtual QString caption() const = 0;
        virtual void processMousePressEvent( QMouseEvent* ) = 0;
        virtual void showWindowMenu( const QRect &) = 0;
        virtual void showWindowMenu( const QPoint & ) = 0;
        virtual void performWindowOperation( WindowOperation ) = 0;
        virtual void setMask( const QRegion&, int ) = 0;
        virtual bool isPreview() const = 0;
        virtual QRect geometry() const = 0;
        virtual QRect iconGeometry() const = 0;
        virtual QRegion unobscuredRegion( const QRegion& r ) const = 0;
        virtual WId windowId() const = 0;
        virtual void closeWindow() = 0;
        virtual void maximize( MaximizeMode mode ) = 0;
        virtual void minimize() = 0;
        virtual void showContextHelp() = 0;
        virtual void setDesktop( int desktop ) = 0;
        virtual void titlebarDblClickOperation() = 0;
        virtual void titlebarMouseWheelOperation( int delta ) = 0;
        virtual void setShade( bool set ) = 0;
        virtual void setKeepAbove( bool ) = 0;
        virtual void setKeepBelow( bool ) = 0;
        // not part of public API
        virtual int currentDesktop() const = 0;
        virtual QWidget* initialParentWidget() const = 0;
        virtual Qt::WFlags initialWFlags() const = 0;
        virtual void grabXServer( bool grab ) = 0;
    };

class KWIN_EXPORT KDecorationBridgeUnstable
    : public KDecorationBridge
    {
    public:
        virtual bool compositingActive() const = 0;
        virtual QRect transparentRect() const = 0;

        // Window tabbing
        virtual bool isClientGroupActive() = 0;
        virtual QList< ClientGroupItem > clientGroupItems() const = 0;
        virtual long itemId( int index ) = 0;
        virtual int visibleClientGroupItem() = 0;
        virtual void setVisibleClientGroupItem( int index ) = 0;
        virtual void moveItemInClientGroup( int index, int before ) = 0;
        virtual void moveItemToClientGroup( long itemId, int before ) = 0;
        virtual void removeFromClientGroup( int index, const QRect& newGeom ) = 0;
        virtual void closeClientGroupItem( int index ) = 0;
        virtual void closeAllInClientGroup() = 0;
        virtual void displayClientMenu( int index, const QPoint& pos ) = 0;
        virtual WindowOperation buttonToWindowOperation( Qt::MouseButtons button ) = 0;
    };

/** @} */

#endif
