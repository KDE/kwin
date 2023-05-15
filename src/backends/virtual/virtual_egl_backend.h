/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include <memory>

namespace KWin
{

class GbmGraphicsBuffer;
class GbmGraphicsBufferAllocator;
class VirtualBackend;
class GLFramebuffer;
class GLTexture;
class VirtualEglBackend;

class VirtualEglLayerBuffer
{
public:
    VirtualEglLayerBuffer(GbmGraphicsBuffer *buffer, VirtualEglBackend *backend);
    ~VirtualEglLayerBuffer();

    GbmGraphicsBuffer *graphicsBuffer() const;
    GLFramebuffer *framebuffer() const;
    std::shared_ptr<GLTexture> texture() const;

private:
    GbmGraphicsBuffer *m_graphicsBuffer;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::shared_ptr<GLTexture> m_texture;
};

class VirtualEglSwapchain
{
public:
    VirtualEglSwapchain(const QSize &size, uint32_t format, VirtualEglBackend *backend);

    QSize size() const;

    std::shared_ptr<VirtualEglLayerBuffer> acquire();

private:
    VirtualEglBackend *m_backend;
    QSize m_size;
    uint32_t m_format;
    std::unique_ptr<GbmGraphicsBufferAllocator> m_allocator;
    QVector<std::shared_ptr<VirtualEglLayerBuffer>> m_buffers;
};

class VirtualEglLayer : public OutputLayer
{
public:
    VirtualEglLayer(Output *output, VirtualEglBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    std::shared_ptr<GLTexture> texture() const;
    quint32 format() const override;

private:
    VirtualEglBackend *const m_backend;
    Output *m_output;
    std::unique_ptr<VirtualEglSwapchain> m_swapchain;
    std::shared_ptr<VirtualEglLayerBuffer> m_current;
};

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class VirtualEglBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    VirtualEglBackend(VirtualBackend *b);
    ~VirtualEglBackend() override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> textureForOutput(Output *output) const override;
    OutputLayer *primaryLayer(Output *output) override;
    void present(Output *output) override;
    void init() override;

    VirtualBackend *backend() const;

private:
    bool initializeEgl();
    bool initRenderingContext();

    void addOutput(Output *output);
    void removeOutput(Output *output);

    VirtualBackend *m_backend;
    std::map<Output *, std::unique_ptr<VirtualEglLayer>> m_outputs;
};

} // namespace KWin
