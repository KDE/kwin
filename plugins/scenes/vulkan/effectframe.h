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

#ifndef EFFECTFRAME_H
#define EFFECTFRAME_H

#include "scene.h"

namespace KWin
{

class VulkanEffectFrame : public Scene::EffectFrame
{
public:
    VulkanEffectFrame(EffectFrameImpl *frame, VulkanScene *scene);
    ~VulkanEffectFrame() override final;

    void render(QRegion region, double opacity, double frameOpacity) override final;

    void free() override final;

    void freeIconFrame() override final;
    void freeTextFrame() override final;
    void freeSelection() override final;

    void crossFadeIcon() override final;
    void crossFadeText() override final;

private:
    VulkanScene *m_scene;
};

} // namespace KWin

#endif // EFFECTFRAME_H
