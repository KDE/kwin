/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_EXPLOSIONEFFECT_H
#define KWIN_EXPLOSIONEFFECT_H

// Include with base class for effects.
#include <effects.h>

namespace KWinInternal
{

class GLShader;

/**
 * Makes windows explode into small pieces when they're closed
 **/
class ExplosionEffect
    : public Effect
    {
    public:
        ExplosionEffect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );


    protected:
        bool loadData();
        unsigned int loadTexture(const QString& filename);
        QImage convertToGLFormat(const QImage& img) const;

    private:
        GLShader* mShader;
        unsigned int mStartOffsetTex;
        unsigned int mEndOffsetTex;
        QMap< const EffectWindow*, double > mWindows;
        int mActiveAnimations;
        bool mValid;
        bool mInited;
    };

} // namespace

#endif
