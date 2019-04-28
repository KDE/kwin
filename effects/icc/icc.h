/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Vitaliy Filippov <vitalif@yourcmc.ru>

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

#ifndef KWIN_ICC_H
#define KWIN_ICC_H

#include <kwineffects.h>
#include "kwinglutils.h"

namespace KWin
{

class GLShader;

/**
 * Applies cLUT (32x32x32 RGB LUT)-based color correction to the whole desktop
 * cLUT is generated from a pair of ICC profiles using LCMS2
 */
class ICCEffect
    : public Effect
{
    Q_OBJECT
public:
    ICCEffect();
    ~ICCEffect();

    virtual void reconfigure(ReconfigureFlags flags);
    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity) override;
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override;

    static bool supported();

public Q_SLOTS:

protected:
    bool loadData();

private:
    bool m_inited;
    bool m_valid;
    QString m_sourceICC;
    QString m_targetICC;
    GLShader* m_shader;
    GLuint m_texture;
    uint8_t *m_clut;

    uint8_t *makeCLUT(const char* source_icc, const char* target_icc);
    GLuint setupCCTexture(uint8_t *clut);
};

inline int ICCEffect::requestedEffectChainPosition() const
{
    return 98;
}

} // namespace

#endif
