/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void postPaintScreen();

        static bool supported();

    protected:
        bool loadData();

    private:
        GLTexture* mTexture;
        GLRenderTarget* mRenderTarget;
        bool mValid;

        float mRot;
    };

} // namespace

#endif
