/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include "outline.h"

namespace KWin
{

KWIN_EFFECT(outline, OutlineEffect)

OutlineEffect::OutlineEffect()
    : Effect()
    , m_active(false)
{
    m_outline = effects->effectFrame(EffectFrameNone);
    connect(effects, SIGNAL(showOutline(QRect)), SLOT(slotShowOutline(QRect)));
    connect(effects, SIGNAL(hideOutline()), SLOT(slotHideOutline()));
}

OutlineEffect::~OutlineEffect()
{
    delete m_outline;
}

void OutlineEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
    if (m_active) {
        m_outline->render();
    }
}

bool OutlineEffect::provides(Feature feature)
{
    if (feature == Outline) {
        return true;
    } else {
        return false;
    }
}


void OutlineEffect::slotHideOutline()
{
    m_active = false;
    effects->addRepaint(m_geometry);
}

void OutlineEffect::slotShowOutline(const QRect& geometry)
{
    if (m_active) {
        effects->addRepaint(m_geometry);
    }
    m_active = true;
    m_geometry = geometry;
    m_outline->setGeometry(geometry);
    m_outline->setSelection(geometry);
    effects->addRepaint(geometry);
}

} // namespace
