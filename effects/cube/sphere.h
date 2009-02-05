/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_SPHERE_H
#define KWIN_SPHERE_H

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class SphereEffect
    : public CubeEffect
    {
    public:
        SphereEffect();
        ~SphereEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        static bool supported();
    protected:
        virtual void paintCap( float z, float zTexture );
        virtual void paintCapStep( float z, float zTexture, bool texture );
    private:
        bool loadData();
        bool mInited;
        bool mValid;
        GLShader* mShader;
        float capDeformationFactor;
    };

} // namespace

#endif
