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

#ifndef KWIN_BLUR_H
#define KWIN_BLUR_H

// Include with base class for effects.
#include <kwineffects.h>

#include <QRegion>

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

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );

        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        static bool supported();

    protected:
        bool loadData();
        GLShader* loadShader(const QString& name);
        void updateBlurTexture(const QVector<QRect>& rects);
        void updateBlurTexture(const QRegion& region);

        QRegion expandedRegion( const QRegion& r ) const;

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

        QRegion mBlurDirty;
        QRegion mScreenDirty;

        QRegion mBlurMask;
    };

} // namespace

#endif
