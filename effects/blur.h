/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_BLUR_H
#define KWIN_BLUR_H

// Include with base class for effects.
#include <kwineffects.h>

template< class T > class QVector;


namespace KWin
{

class GLRenderTarget;
class GLTexture;
class GLShader;

/**
 * Blurs the background of translucent windows
 **/
class BlurEffect : public Effect
    {
    public:
        BlurEffect();
        ~BlurEffect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );

        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        static bool supported();

    protected:
        bool loadData();
        GLShader* loadShader(const QString& name);
        void updateBlurTexture(const QVector<QRect>& rects);
        void updateBlurTexture(const QRegion& region);

    private:
        GLTexture* mSceneTexture;
        GLTexture* mTmpTexture;
        GLTexture* mBlurTexture;
        GLRenderTarget* mSceneTarget;
        GLRenderTarget* mTmpTarget;
        GLRenderTarget* mBlurTarget;
        GLShader* mBlurShader;
        GLShader* mWindowShader;
        bool mValid;
        int mBlurRadius;

        int mTransparentWindows;
        int mTime;
    };

} // namespace

#endif
