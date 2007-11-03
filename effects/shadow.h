/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHADOW_H
#define KWIN_SHADOW_H

#include <kwineffects.h>


namespace KWin
{

class GLTexture;

class ShadowEffect
    : public Effect
    {
    public:
        ShadowEffect();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );
        virtual void windowClosed( EffectWindow* c );
    private:
        void drawShadow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        void addQuadVertices(QVector<float>& verts, float x1, float y1, float x2, float y2) const;
        // transforms window rect -> shadow rect
        QRect shadowRectangle(const QRect& windowRectangle) const;
        bool useShadow( EffectWindow* w ) const;

        int shadowXOffset, shadowYOffset;
        double shadowOpacity;
        int shadowFuzzyness;
        int shadowSize;
        bool intensifyActiveShadow;
        GLTexture* mShadowTexture;
    };

} // namespace

#endif
