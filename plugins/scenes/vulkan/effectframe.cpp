/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

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

#include "effectframe.h"

namespace KWin
{


VulkanEffectFrame::VulkanEffectFrame(EffectFrameImpl *frame, VulkanScene *scene)
    : Scene::EffectFrame(frame),
      m_scene(scene)
{
}


VulkanEffectFrame::~VulkanEffectFrame()
{
}


void VulkanEffectFrame::render(QRegion region, double opacity, double frameOpacity)
{
    Q_UNUSED(region)
    Q_UNUSED(opacity)
    Q_UNUSED(frameOpacity)
}


// Called when the geometry has changed, the plasma theme has changed, or an effect has called free()
void VulkanEffectFrame::free()
{
}


// Called when the icon and/or icon size has changed
void VulkanEffectFrame::freeIconFrame()
{
}


// Called when the geometry has changed as a result of a font and/or text change
void VulkanEffectFrame::freeTextFrame()
{
}


// Called when the geometry of the selection has changed
void VulkanEffectFrame::freeSelection()
{
}


// Called when cross-fading is enabled, and the icon is about to be changed
void VulkanEffectFrame::crossFadeIcon()
{
}


// Called when cross-fading is enabled, and the text is about to be changed
void VulkanEffectFrame::crossFadeText()
{
}


} // namespace KWin
