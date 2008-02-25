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

#ifndef KWIN_SCENE_XRENDER_H
#define KWIN_SCENE_XRENDER_H

#include <config-workspace.h>

#include "scene.h"

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xfixes.h>

namespace KWin
{


class SceneXrender
    : public Scene
    {
    public:
        SceneXrender( Workspace* ws );
        virtual ~SceneXrender();
        virtual bool initFailed() const;
        virtual CompositingType compositingType() const { return XRenderCompositing; }
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
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        static ScreenPaintData screen_paint;
        class Window;
        QHash< Toplevel*, Window* > windows;
        bool init_ok;
    };

class SceneXrender::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual ~Window();
        virtual void performPaint( int mask, QRegion region, WindowPaintData data );
        void discardPicture();
        void discardAlpha();
        QRegion transformedShape() const;
        void setTransformedShape( const QRegion& shape );
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
