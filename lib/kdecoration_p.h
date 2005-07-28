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

#ifndef KDECORATION_P_H
#define KDECORATION_P_H

//
// This header file is internal. I mean it.
// 

#include "kdecoration.h"
#include <qwidget.h>

class KConfig;

class KWIN_EXPORT KDecorationOptionsPrivate : public KDecorationDefines
    {
    public:
        KDecorationOptionsPrivate();
        virtual ~KDecorationOptionsPrivate();
        void defaultKWinSettings(); // shared implementation
        unsigned long updateKWinSettings( KConfig* ); // shared implementation
        BorderSize findPreferredBorderSize( BorderSize size, QList< BorderSize > ) const; // shared implementation

        QColor colors[NUM_COLORS*2];
        QColorGroup *cg[NUM_COLORS*2];
        QFont activeFont, inactiveFont, activeFontSmall, inactiveFontSmall;
        QString title_buttons_left;
        QString title_buttons_right;
        bool custom_button_positions;
        bool show_tooltips;
        BorderSize border_size, cached_border_size;
        bool move_resize_maximized_windows;
        WindowOperation OpMaxButtonRightClick;
        WindowOperation OpMaxButtonMiddleClick;
        WindowOperation OpMaxButtonLeftClick;
    };

class KDecorationBridge : public KDecorationDefines
    {
    public:
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
	virtual QIconSet icon() const = 0;
	virtual QString caption() const = 0;
	virtual void processMousePressEvent( QMouseEvent* ) = 0;
	virtual void showWindowMenu( const QRect &) = 0;
	virtual void showWindowMenu( QPoint ) = 0;
	virtual void performWindowOperation( WindowOperation ) = 0;
        virtual void setMask( const QRegion&, int ) = 0;
        virtual bool isPreview() const = 0;
        virtual QRect geometry() const = 0;
        virtual QRect iconGeometry() const = 0;
        virtual QRegion unobscuredRegion( const QRegion& r ) const = 0;
        virtual QWidget* workspaceWidget() const = 0;
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
        virtual void helperShowHide( bool ) = 0;
        virtual void grabXServer( bool grab ) = 0;
    };

#endif
