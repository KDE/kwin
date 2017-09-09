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

#ifndef WINDOWPIXMAP_H
#define WINDOWPIXMAP_H

#include "../../../scene.h"

#include <QPointer>
#include <memory>

namespace KWayland {
    namespace Server {
        class SubSurfaceInterface;
    }
}

namespace KWin
{

class VulkanScene;
class VulkanImage;
class VulkanImageView;
class VulkanDeviceMemory;

class VulkanWindowPixmap : public WindowPixmap
{
public:
    explicit VulkanWindowPixmap(Scene::Window *window, VulkanScene *scene);

    explicit VulkanWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface,
                                WindowPixmap *parent,
                                VulkanScene *scene);

    ~VulkanWindowPixmap() override final;

    void create() override final;

    VulkanScene *scene() const { return m_scene; }

    WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface);

    bool isValid() const override final;

    void aboutToRender();

private:
    VulkanScene *m_scene;
};

} // namespace KWin

#endif // WINDOWPIXMAP_H
