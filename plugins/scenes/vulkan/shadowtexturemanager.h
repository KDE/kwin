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

#include "kwinvulkanutils.h"

#include <unordered_map>

#include <QImage>


namespace std
{
    template <>
    struct hash<QImage>
    {
        using argument_type = QImage;
        using result_type = size_t;

        size_t operator()(const QImage &) const;
    };
}


namespace KWin {


struct ShadowData
{
    std::shared_ptr<VulkanImage> image;
    std::shared_ptr<VulkanImageView> imageView;
    std::shared_ptr<VulkanDeviceMemory> memory;
};


class VulkanScene;


class ShadowTextureManager
{
    struct InternalShadowData : public ShadowData
    {
        uint64_t id;
    };

    struct HashTableEntry
    {
        uint64_t id;
        std::weak_ptr<ShadowData> data;
    };

public:
    ShadowTextureManager(VulkanScene *scene);
    ~ShadowTextureManager();

    VulkanScene *scene() const { return m_scene; }

    std::shared_ptr<ShadowData> lookup(const QImage &image);

private:
    std::shared_ptr<InternalShadowData> createTexture(const QImage &image);
    void remove(InternalShadowData *data);

private:
    VulkanScene *m_scene;
    std::unordered_map<QImage, HashTableEntry> m_hashTable;
    uint64_t m_serial = 0;
};

} // namespace KWin
