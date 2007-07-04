/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHADEREFFECT_H
#define KWIN_SHADEREFFECT_H


#include <kwineffects.h>


namespace KWin
{

class GLTexture;
class GLRenderTarget;
class GLShader;
class KWIN_EXPORT ShaderEffect : public Effect
{
    public:
        ShaderEffect(const QString& shadername);
        virtual ~ShaderEffect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void postPaintScreen();

        static bool supported();

    protected:
        bool isEnabled() const;
        void setEnabled(bool enabled);

        GLShader* shader() const;

        bool loadData(const QString& shadername);

    private:
        GLTexture* mTexture;
        GLRenderTarget* mRenderTarget;
        GLShader* mShader;
        bool mValid;

        float mTime;

        bool mEnabled;
};

} // namespace

#endif
