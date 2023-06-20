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

class EglSwapchainSlot;
class EglSwapchain;
class GraphicsBufferAllocator;
class VirtualBackend;
class GLFramebuffer;
class GLTexture;
class VirtualEglBackend;

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
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_current;
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
    GraphicsBufferAllocator *graphicsBufferAllocator() const;

private:
    bool initializeEgl();
    bool initRenderingContext();

    void addOutput(Output *output);
    void removeOutput(Output *output);

    VirtualBackend *m_backend;
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, std::unique_ptr<VirtualEglLayer>> m_outputs;
};

} // namespace KWin
