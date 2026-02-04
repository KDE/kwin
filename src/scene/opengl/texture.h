/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/texture.h"

#include <QImage>
#include <QVarLengthArray>

namespace KWin
{

class EglBackend;
class GLTexture;
class GraphicsBuffer;
class Region;
class Rect;

class TextureOpenGL : public Texture
{
public:
    ~TextureOpenGL() override;

    QVarLengthArray<GLTexture *, 4> planes() const;

protected:
    QVarLengthArray<GLTexture *, 4> m_planes;
};

class ImageTextureOpenGL : public TextureOpenGL
{
public:
    static std::unique_ptr<ImageTextureOpenGL> create(const QImage &image);

    void attach(GraphicsBuffer *buffer, const Region &region) override;

    bool upload(const QImage &image);
    void upload(const QImage &image, const Rect &region) override;
};

class BufferTextureOpenGL : public TextureOpenGL
{
public:
    static std::unique_ptr<BufferTextureOpenGL> create(GraphicsBuffer *buffer);

    explicit BufferTextureOpenGL(EglBackend *backend);

    bool attach(GraphicsBuffer *buffer);
    void attach(GraphicsBuffer *buffer, const Region &region) override;

    void upload(const QImage &image, const Rect &region) override;

private:
    void reset();

    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const Region &region);
    bool loadDmabufTexture(GraphicsBuffer *buffer);
    void updateDmabufTexture(GraphicsBuffer *buffer);
    bool loadSinglePixelTexture(GraphicsBuffer *buffer);
    void updateSinglePixelTexture(GraphicsBuffer *buffer);

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        SinglePixel,
    };

    BufferType m_bufferType = BufferType::None;
    EglBackend *m_backend;
};

} // namespace KWin
