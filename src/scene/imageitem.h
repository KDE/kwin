/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class GLTexture;

class KWIN_EXPORT ImageItem : public Item
{
    Q_OBJECT

public:
    explicit ImageItem(Scene *scene, Item *parent = nullptr);

    QImage image() const;
    void setImage(const QImage &image);

protected:
    QImage m_image;
};

class ImageItemOpenGL : public ImageItem
{
    Q_OBJECT

public:
    explicit ImageItemOpenGL(Scene *scene, Item *parent = nullptr);
    ~ImageItemOpenGL() override;

    GLTexture *texture() const;

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    std::unique_ptr<GLTexture> m_texture;
    qint64 m_textureKey = 0;
};

} // namespace KWin
