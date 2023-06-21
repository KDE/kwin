/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

#include <QImage>

namespace KWin
{

class GLTexture;
class VulkanTexture;

class KWIN_EXPORT ImageItem : public Item
{
    Q_OBJECT

public:
    explicit ImageItem(Item *parent = nullptr);

    QImage image() const;
    void setImage(const QImage &image);

protected:
    QImage m_image;
};

class ImageItemOpenGL : public ImageItem
{
    Q_OBJECT

public:
    explicit ImageItemOpenGL(Item *parent = nullptr);
    ~ImageItemOpenGL() override;

    GLTexture *texture() const;

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    std::unique_ptr<GLTexture> m_texture;
    qint64 m_textureKey = 0;
};

class ImageItemVulkan : public ImageItem
{
public:
    explicit ImageItemVulkan(Item *parent = nullptr);
    ~ImageItemVulkan() override;

    VulkanTexture *texture() const;

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    std::unique_ptr<VulkanTexture> m_texture;
    std::optional<int64_t> m_textureKey;
};

} // namespace KWin
