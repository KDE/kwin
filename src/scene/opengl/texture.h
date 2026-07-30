/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/texture.h"

#include <QImage>
#include <QVarLengthArray>
#include <sys/types.h>

namespace KWin
{

class RenderDevice;
class GLTexture;
class GraphicsBuffer;
class Region;
class Rect;
typedef void *EGLImageKHR;
class MultiGpuSwapchain;
class EglContext;

class TextureOpenGL : public Texture
{
public:
    explicit TextureOpenGL(const std::shared_ptr<EglContext> &context);
    ~TextureOpenGL() override;

    QVarLengthArray<GLTexture *, 4> planes() const;

protected:
    QVarLengthArray<GLTexture *, 4> m_planes;

    std::shared_ptr<EglContext> m_context;
};

class ImageTextureOpenGL : public TextureOpenGL
{
public:
    static std::unique_ptr<ImageTextureOpenGL> create(const std::shared_ptr<EglContext> &context, const QImage &image);

    explicit ImageTextureOpenGL(const std::shared_ptr<EglContext> &context);

    void attach(GraphicsBuffer *buffer, const FileDescriptor &sync, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint) override;

    bool upload(const QImage &image);
    void upload(const QImage &image, const Rect &region) override;
};

class BufferTextureOpenGL : public TextureOpenGL
{
public:
    static std::unique_ptr<BufferTextureOpenGL> create(RenderDevice *device, GraphicsBuffer *buffer, const FileDescriptor &sync, const std::shared_ptr<SyncReleasePoint> &releasePoint);

    explicit BufferTextureOpenGL(RenderDevice *device);
    ~BufferTextureOpenGL() override;

    bool attach(GraphicsBuffer *buffer, const FileDescriptor &sync, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    void attach(GraphicsBuffer *buffer, const FileDescriptor &sync, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint) override;

    void upload(const QImage &image, const Rect &region) override;

private:
    void reset();

    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const Region &region);
    bool loadDmabufTexture(GraphicsBuffer *buffer, const FileDescriptor &sync, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    void updateDmabufTexture(GraphicsBuffer *buffer, const FileDescriptor &sync, const Region &region, const std::shared_ptr<SyncReleasePoint> &releasePoint);
    bool loadSinglePixelTexture(GraphicsBuffer *buffer);
    void updateSinglePixelTexture(GraphicsBuffer *buffer);
    bool loadUDmabufTexture(GraphicsBuffer *buffer, EGLImageKHR image);
    void updateUDmabufTexture(GraphicsBuffer *buffer, EGLImageKHR image);

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        SinglePixel,
        UDmaBuf,
    };

    BufferType m_bufferType = BufferType::None;
    RenderDevice *m_renderDevice;
    std::unique_ptr<MultiGpuSwapchain> m_mgpuSwapchain;
    std::optional<dev_t> m_dmabufDevice;
};

} // namespace KWin
