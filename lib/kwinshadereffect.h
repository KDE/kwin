/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_SHADEREFFECT_H
#define KWIN_SHADEREFFECT_H


#include <kwineffects.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING

/** @addtogroup kwineffects */
/** @{ */

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

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
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

        double mTime;

        bool mEnabled;
};

} // namespace

/** @} */

#endif

#endif
