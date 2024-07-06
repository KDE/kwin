/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include <chrono>
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
class GLRenderTimeQuery;

class VirtualEglLayer : public OutputLayer
{
public:
    VirtualEglLayer(Output *output, VirtualEglBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    std::shared_ptr<GLTexture> texture() const;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    VirtualEglBackend *const m_backend;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_current;
    std::unique_ptr<GLRenderTimeQuery> m_query;
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
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap) override;
    std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> textureForOutput(Output *output) const override;
    OutputLayer *primaryLayer(Output *output) override;
    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    void init() override;

    VirtualBackend *backend() const;
    DrmDevice *drmDevice() const override;

private:
    bool initializeEgl();
    bool initRenderingContext();

    void addOutput(Output *output);
    void removeOutput(Output *output);

    VirtualBackend *m_backend;
    std::map<Output *, std::unique_ptr<VirtualEglLayer>> m_outputs;
};

} // namespace KWin
