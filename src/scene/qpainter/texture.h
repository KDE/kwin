/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/texture.h"

#include <QImage>

namespace KWin
{

class GraphicsBuffer;
class Rect;
class Region;

class TextureQPainter : public Texture
{
public:
    QImage nativeImage() const;

protected:
    QImage m_image;
};

class ImageTextureQPainter : public TextureQPainter
{
public:
    static std::unique_ptr<ImageTextureQPainter> create(const QImage &image);

    void attach(GraphicsBuffer *buffer, const Region &region) override;

    void upload(const QImage &image);
    void upload(const QImage &image, const Rect &rect) override;
};

class BufferTextureQPainter : public TextureQPainter
{
public:
    static std::unique_ptr<BufferTextureQPainter> create(GraphicsBuffer *buffer);

    bool attach(GraphicsBuffer *buffer);
    void attach(GraphicsBuffer *buffer, const Region &region) override;

    void upload(const QImage &image, const Rect &rect) override;
};

} // namespace KWin
