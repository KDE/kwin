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

#ifndef KWIN_DEMO_LIQUID_H
#define KWIN_DEMO_LIQUID_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class GLRenderTarget;
class GLTexture;
class GLShader;

/**
 * Turns your desktop into a wavy (liquid) surface
 **/
class LiquidEffect : public Effect
    {
    public:
        LiquidEffect();
        ~LiquidEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void postPaintScreen();

        static bool supported();

    protected:
        bool loadData();

    private:
        GLTexture* mTexture;
        GLRenderTarget* mRenderTarget;
        GLShader* mShader;
        bool mValid;

        double mTime;
    };

} // namespace

#endif
