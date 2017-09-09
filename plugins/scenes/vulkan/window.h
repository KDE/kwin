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

#ifndef VULKAN_WINDOW_H
#define VULKAN_WINDOW_H

#include "scene.h"

namespace KWin
{

class VulkanWindow : public Scene::Window
{
public:
    explicit VulkanWindow(Toplevel *toplevel, VulkanScene *scene);
    ~VulkanWindow() override;

    void performPaint(int mask, QRegion region, WindowPaintData data) override;

    VulkanScene *scene() const { return m_scene; }

protected:
    virtual WindowPixmap *createWindowPixmap();

private:
    VulkanPipelineManager *pipelineManager() { return scene()->pipelineManager(); }
    VulkanUploadManager *uploadManager() { return scene()->uploadManager(); }

private:
    VulkanScene *m_scene;
};

} // namespace KWin

#endif // VULKAN_WINDOW_H
