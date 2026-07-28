/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

#include <QImage>

namespace KWin
{

class Texture;
class RenderDevice;

class KWIN_EXPORT ImageItem : public Item
{
    Q_OBJECT

public:
    explicit ImageItem(Item *parent = nullptr);
    ~ImageItem() override;

    Texture *texture(RenderDevice *device) const;

    QImage image() const;
    void setImage(const QImage &image);

protected:
    void preprocess(ItemRenderer *renderer) override;
    WindowQuadList buildQuads(ItemRenderer *renderer) const override;
    void releaseResources(RenderDevice *device) override;

    QImage m_image;
    std::unordered_map<RenderDevice *, std::pair<std::unique_ptr<Texture>, int64_t>> m_textures;
};

} // namespace KWin
