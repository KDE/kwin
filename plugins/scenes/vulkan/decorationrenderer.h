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

#ifndef DECORATIONRENDERER_H
#define DECORATIONRENDERER_H

#include "scene.h"

namespace KWin
{

class VulkanScene;

class VulkanDecorationRenderer : public Decoration::Renderer
{
    Q_OBJECT

public:
    enum class DecorationPart : int {
        Left = 0,
        Top,
        Right,
        Bottom,
        Count
    };

    explicit VulkanDecorationRenderer(Decoration::DecoratedClientImpl *client, VulkanScene *scene);
    ~VulkanDecorationRenderer() override;

    VulkanScene *scene() const { return m_scene; }

    void render() override;
    void reparent(Deleted *deleted) override;

private:
    VulkanScene *m_scene;
};

} // namespace KWin

#endif // DECORATIONRENDERER_H
