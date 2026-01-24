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
class MultiGpuSwapchain;

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

    void attach(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint) override;

    bool upload(const QImage &image);
    void upload(const QImage &image, const Rect &region) override;
};

class BufferTextureOpenGL : public TextureOpenGL
{
public:
    static std::unique_ptr<BufferTextureOpenGL> create(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint);

    explicit BufferTextureOpenGL(EglBackend *backend);
    ~BufferTextureOpenGL() override;

    bool attach(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    void attach(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint) override;

    void upload(const QImage &image, const Rect &region) override;

private:
    void reset();

    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    bool loadDmabufTexture(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    void updateDmabufTexture(GraphicsBuffer *buffer, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    bool loadSinglePixelTexture(GraphicsBuffer *buffer);
    void updateSinglePixelTexture(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint);

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        SinglePixel,
    };

    BufferType m_bufferType = BufferType::None;
    EglBackend *m_backend;
    std::unique_ptr<MultiGpuSwapchain> m_mgpuSwapchain;
    std::optional<dev_t> m_dmabufDevice;
};

} // namespace KWin
