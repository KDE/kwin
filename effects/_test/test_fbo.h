/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_TESTFBOEFFECT_H
#define KWIN_TESTFBOEFFECT_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class GLRenderTarget;
class GLTexture;

/**
 * Demonstrates usage of GLRenderTarget by first rendering the scene onto a
 *  texture and then rendering a small rotating thumbnail of the entire scene
 *  on top of the usual scene.
 **/
class TestFBOEffect : public Effect
    {
    public:
        TestFBOEffect();
        ~TestFBOEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void postPaintScreen();

        static bool supported();

    private:
        GLTexture* mTexture;
        GLRenderTarget* mRenderTarget;
        bool mValid;

        double mRot;
    };

} // namespace

#endif
