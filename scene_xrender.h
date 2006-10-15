/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_XRENDER_H
#define KWIN_SCENE_XRENDER_H

#include "config.h"

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>

#include "scene.h"
#include "effects.h"
#include "toplevel.h"

namespace KWinInternal
{

class Matrix;

class SceneXrender
    : public Scene
    {
    public:
        SceneXrender( Workspace* ws );
        virtual ~SceneXrender();
        virtual void paint( QRegion damage, ToplevelList windows );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    private:
        void createBuffer();
        void paintGenericScreen( ToplevelList windows );
        void paintSimpleScreen( QRegion damage, ToplevelList windows );
        void paintBackground( QRegion region );
        static QRegion infiniteRegion();
        static XserverRegion toXserverRegion( QRegion region );
        enum
            {
            PAINT_OPAQUE = 1 << 0,
            PAINT_TRANSLUCENT = 1 << 1
            };
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        class Window;
        QMap< Toplevel*, Window > windows;
        struct Phase2Data
            {
            Phase2Data( Window* w, QRegion r ) : window( w ), region( r ) {}
            Window* window;
            QRegion region;
            };
    };

class SceneXrender::Window
    {
    public:
        Window( Toplevel* c );
        void free(); // is often copied by value, use manually instead of dtor
        int x() const;
        int y() const;
        int width() const;
        int height() const;
        void paint( QRegion region, int mask );
        bool isOpaque() const;
        void geometryShapeChanged();
        void opacityChanged();
        bool isVisible() const;
        QRegion shape() const;
        void discardPicture();
        void discardShape();
        void discardAlpha();
        Window() {} // QMap sucks even in Qt4
    private:
        Picture picture();
        Picture alphaMask();
        Toplevel* toplevel;
        Picture _picture;
        XRenderPictFormat* format;
        Picture alpha;
        double alpha_cached_opacity;
        mutable QRegion shape_region;
        mutable bool shape_valid;
    };

inline
QRegion SceneXrender::infiniteRegion()
    { // INT_MIN / 2 because it's width/height (INT_MIN+INT_MAX==-1)
    return QRegion( INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX );
    }

inline
int SceneXrender::Window::x() const
    {
    return toplevel->x();
    }
    
inline
int SceneXrender::Window::y() const
    {
    return toplevel->y();
    }

inline
int SceneXrender::Window::width() const
    {
    return toplevel->width();
    }
    
inline
int SceneXrender::Window::height() const
    {
    return toplevel->height();
    }
    
} // namespace

#endif

#endif
