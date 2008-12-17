/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_TILING_H
#define KWIN_TILING_H

#include <kwineffects.h>
#include <QObject>

namespace KWin
{

class TilingWindow;
class TilingSurface;
class TilingFrame;

enum TilingMode
    {
    TilingModeNone,
    TilingModeAddRemove, // Add/remove without changing the grid
    TilingModeSplitMerge // Add/remove and change the grid
    };

class TilingEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        static QHash<EffectWindow*, TilingWindow*> m_tilingData;

        TilingEffect();
        virtual void reconfigure( ReconfigureFlags );

        //virtual void windowActivated( EffectWindow *w );
        virtual void windowAdded( EffectWindow *w );
        virtual void windowClosed( EffectWindow *w );
        //virtual void windowMinimized( EffectWindow *w );
        //virtual void windowUnminimized( EffectWindow *w );
        virtual void windowUserMovedResized( EffectWindow *w, bool first, bool last );

    public slots:
        void frameAdd();
        void frameSplitAdd();

    private:
        void enterTilingMode( TilingMode mode );
        void exitTilingMode();
        void doTilingSwitch( EffectWindow *w );//QPoint mousePos );
        bool isInTilingSwitch() const; // Are we switching between floating and tiling?

        TilingMode m_tilingMode;
    };

class TilingWindow
    {
    public:
        TilingWindow( EffectWindow *w ) : m_window( w ), m_tilingFrame( NULL ) {};

        TilingFrame* tilingFrame() const;
        void setTilingFrame( TilingFrame *newFrame );
        bool isTiled() const;
        bool isTiled( TilingSurface *surface ) const;

    private:
        EffectWindow *m_window;
        TilingFrame* m_tilingFrame;
    };

class TilingSurface
    {
    public:
        // TODO: Do we really need three?
        static TilingSurface* getSurface( int desktop, int screen );
        static TilingSurface* makeSurface( int desktop, int screen );
        static TilingSurface* getOrMakeSurface( int desktop, int screen );

        void addToSurface( EffectWindow *window, QPoint pos, TilingMode mode = TilingModeAddRemove );
        void removeFromSurface( EffectWindow *window, TilingMode mode = TilingModeSplitMerge );

        TilingFrame* getFrame( QPoint pos ) const;
        TilingFrame* getFrame( QRect geometry ) const;

        void resizedWindow( EffectWindow *window );
        // Only TilingFrame should use this, therefore TODO: Move somewhere else?
        void resizeFrame( TilingFrame *frame, KDecorationDefines::Position mode, QRect newGeometry );

        QRect geometry() const;
        int desktop() const;
        void setDesktop( int newDesktop );
        int screen() const;
        void setScreen( int newScreen );
        QList<EffectWindow*> windowsOnSurface() const;

    private:
        TilingSurface( int desktop, int screen );

        // Keep track of all surfaces so we can move them between desktops
        static QList<TilingSurface*> m_surfaces;

        int m_desktop;
        int m_screen;
        QList<TilingFrame*> m_frames;
        QList<EffectWindow*> m_windows; // TODO: Deprecated by TilingFrame?
    };

class TilingFrame: public QRect
    {
    public:
        TilingFrame( TilingSurface *surface, QRect geometry );

        void resize( KDecorationDefines::Position mode, QRect newGeometry );
        void resized();

        TilingSurface* surface() const;
        void setGeometry( QRect newGeometry );

    private:
        TilingSurface *m_surface;
        //QList<EffectWindow*> m_windows; // TODO
    };

//-----------------------------------------------------------------------------
// TilingEffect

inline void TilingEffect::enterTilingMode( TilingMode mode )
    {
    m_tilingMode = mode;
    }

inline void TilingEffect::exitTilingMode()
    {
    m_tilingMode = TilingModeNone;
    }

inline bool TilingEffect::isInTilingSwitch() const
    {
    return m_tilingMode != TilingModeNone;
    }

//-----------------------------------------------------------------------------
// TilingWindow

inline TilingFrame* TilingWindow::tilingFrame() const
    {
    return m_tilingFrame;
    }

inline void TilingWindow::setTilingFrame( TilingFrame *newFrame )
    {
    m_tilingFrame = newFrame;
    }

inline bool TilingWindow::isTiled() const
    {
    return m_tilingFrame != NULL;
    }

inline bool TilingWindow::isTiled( TilingSurface *surface ) const
    {
    return m_tilingFrame->surface() == surface;
    }

//-----------------------------------------------------------------------------
// TilingSurface

inline TilingFrame* TilingSurface::getFrame( QRect geometry ) const
    {
    return getFrame( geometry.center() );
    }

inline QRect TilingSurface::geometry() const
    {
    return effects->clientArea( MaximizeArea, m_screen, m_desktop );
    }

inline int TilingSurface::desktop() const
    {
    return m_desktop;
    }

inline void TilingSurface::setDesktop( int newDesktop )
    {
    m_desktop = newDesktop;
    }

inline int TilingSurface::screen() const
    {
    return m_screen;
    }

inline void TilingSurface::setScreen( int newScreen )
    {
    m_screen = newScreen;
    }

inline QList<EffectWindow*> TilingSurface::windowsOnSurface() const
    {
    return m_windows;
    }

//-----------------------------------------------------------------------------
// TilingFrame

inline void TilingFrame::resize( KDecorationDefines::Position mode, QRect newGeometry )
    {
    m_surface->resizeFrame( this, mode, newGeometry );
    }

inline TilingSurface* TilingFrame::surface() const
    {
    return m_surface;
    }

inline void TilingFrame::setGeometry( QRect newGeometry )
    {
    setRect( newGeometry.x(), newGeometry.y(), newGeometry.width(), newGeometry.height() );
    resized();
    }

} // namespace

#endif
