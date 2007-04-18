/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void postPaintScreen();

        static bool supported();

    protected:
        bool loadData();

    private:
        GLTexture* mTexture;
        GLRenderTarget* mRenderTarget;
        GLShader* mShader;
        bool mValid;

        float mTime;
    };

} // namespace

#endif
