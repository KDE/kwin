/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef KWINDECORATION_PREVIEW_H
#define KWINDECORATION_PREVIEW_H

#include <qwidget.h>
#include <kdecoration_p.h>
#include <kdecoration_plugins_p.h>

class QLabel;

class KDecorationPreviewBridge;
class KDecorationPreviewOptions;

class KDecorationPreview
    : public QWidget
    {
    public:
        KDecorationPreview( KDecorationPlugins* plugin, QWidget* parent = NULL, const char* name = NULL );
        virtual ~KDecorationPreview();

        void performRepaintTest(int n);
        void performCaptionTest(int n);
        void performResizeTest(int n);
        void performRecreationTest(int n);

        bool recreateDecoration();
        void setPreviewMask( const QRegion&, int );
        QRegion unobscuredRegion( bool, const QRegion& ) const;
        QRect windowGeometry( bool ) const;
    private:
        void positionPreviews(int shrink = 0);
        KDecorationPreviewOptions* options;
        KDecorationPreviewBridge* bridge;
        KDecoration* deco;
        KDecorationPlugins* m_plugin;
    };

class KDecorationPreviewBridge
    : public KDecorationBridge
    {
    public:
        KDecorationPreviewBridge( KDecorationPreview* preview, bool active, const QString &caption );

        void setCaption(const QString &caption);

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
	virtual QIconSet icon() const;
	virtual QString caption() const;
	virtual void processMousePressEvent( QMouseEvent* );
	virtual void showWindowMenu( const QRect &);
	virtual void showWindowMenu( QPoint );
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
        virtual void setShade( bool set );
        virtual void setKeepAbove( bool );
        virtual void setKeepBelow( bool );
        virtual int currentDesktop() const;
        virtual QWidget* initialParentWidget() const;
        virtual Qt::WFlags initialWFlags() const;
        virtual void helperShowHide( bool show );
        virtual void grabXServer( bool grab );
    private:
        KDecorationPreview* preview;
        bool active;
        QString m_caption;
    };

class KDecorationPreviewOptions
    : public KDecorationOptions
    {
    public:
        KDecorationPreviewOptions();
        virtual ~KDecorationPreviewOptions();
        virtual unsigned long updateSettings();

    private:
    };

class KDecorationPreviewPlugins
    : public KDecorationPlugins
    {
    public:
        KDecorationPreviewPlugins( KConfig* cfg );
        virtual bool provides( Requirement );
    };

inline KDecorationPreviewPlugins::KDecorationPreviewPlugins( KConfig* cfg )
    : KDecorationPlugins( cfg )
    {
    }

#endif
