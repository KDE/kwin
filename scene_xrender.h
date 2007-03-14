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

#include "scene.h"

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>

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
        virtual void windowClosed( Toplevel*, Deleted* );
        virtual void windowDeleted( Deleted* );
        Picture bufferPicture();
    protected:
        virtual void paintBackground( QRegion region );
        virtual void paintGenericScreen( int mask, ScreenPaintData data );
    private:
        void paintTransformedScreen( int mask );
        void createBuffer();
        static XserverRegion toXserverRegion( QRegion region );
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        static ScreenPaintData screen_paint;
        class Window;
        QHash< Toplevel*, Window > windows;
    };

class SceneXrender::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual void free();
        virtual void performPaint( int mask, QRegion region, WindowPaintData data );
        void discardPicture();
        void discardAlpha();
        QRegion transformedShape() const;
        void setTransformedShape( const QRegion& shape );
        Window() {} // QMap sucks even in Qt4
    private:
        Picture picture();
        Picture alphaMask( double opacity );
        Picture _picture;
        XRenderPictFormat* format;
        Picture alpha;
        double alpha_cached_opacity;
        QRegion transformed_shape;
    };

inline
Picture SceneXrender::bufferPicture()
    {
    return buffer;
    }

inline
QRegion SceneXrender::Window::transformedShape() const
    {
    return transformed_shape;
    }

inline
void SceneXrender::Window::setTransformedShape( const QRegion& shape )
    {
    transformed_shape = shape;
    }

} // namespace

#endif

#endif
