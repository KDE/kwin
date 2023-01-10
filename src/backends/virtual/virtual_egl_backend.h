/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_egl_backend.h"
#include "core/outputlayer.h"

namespace KWin
{
class VirtualBackend;
class GLFramebuffer;
class GLTexture;
class VirtualEglBackend;

class VirtualEglLayer : public OutputLayer
{
public:
    VirtualEglLayer(Output *output, VirtualEglBackend *backend);
    ~VirtualEglLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    GLTexture *texture() const;

private:
    VirtualEglBackend *const m_backend;
    Output *m_output;
    std::unique_ptr<GLFramebuffer> m_fbo;
    std::unique_ptr<GLTexture> m_texture;
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
    OutputLayer *primaryLayer(Output *output) override;
    void present(Output *output) override;
    void init() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    void addOutput(Output *output);
    void removeOutput(Output *output);

    VirtualBackend *m_backend;
    std::map<Output *, std::unique_ptr<VirtualEglLayer>> m_outputs;
};

} // namespace KWin
