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

class KWIN_EXPORT ImageItem : public Item
{
    Q_OBJECT

public:
    explicit ImageItem(Item *parent = nullptr);
    ~ImageItem() override;

    Texture *texture() const;

    QImage image() const;
    void setImage(const QImage &image);

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;
    void releaseResources() override;

    QImage m_image;
    std::unique_ptr<Texture> m_texture;
    qint64 m_textureKey = 0;
};

} // namespace KWin
