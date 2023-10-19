/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "libkwineffects/glutils.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"

namespace KWin
{

class EglSwapchainSlot;
class EglSwapchain;
class GraphicsBufferAllocator;
class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;
class GLRenderTimeQuery;

class X11WindowedEglPrimaryLayer : public OutputLayer
{
public:
    X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output);
    ~X11WindowedEglPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    quint32 format() const override;
    std::chrono::nanoseconds queryRenderTime() const override;

    std::shared_ptr<GLTexture> texture() const;
    void present();

private:
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_buffer;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    X11WindowedOutput *const m_output;
    X11WindowedEglBackend *const m_backend;
};

class X11WindowedEglCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    X11WindowedEglCursorLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output);
    ~X11WindowedEglCursorLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    quint32 format() const override;
    std::chrono::nanoseconds queryRenderTime() const override;

private:
    X11WindowedOutput *const m_output;
    X11WindowedEglBackend *const m_backend;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLRenderTimeQuery> m_query;
};

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class X11WindowedEglBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    explicit X11WindowedEglBackend(X11WindowedBackend *backend);
    ~X11WindowedEglBackend() override;

    X11WindowedBackend *backend() const;
    GraphicsBufferAllocator *graphicsBufferAllocator() const override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap) override;
    std::pair<std::shared_ptr<GLTexture>, ColorDescription> textureForOutput(Output *output) const override;
    void init() override;
    void endFrame(Output *output, const QRegion &renderedRegion, const QRegion &damagedRegion);
    void present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

protected:
    void cleanupSurfaces() override;

private:
    bool initializeEgl();
    bool initRenderingContext();

    struct Layers
    {
        std::unique_ptr<X11WindowedEglPrimaryLayer> primaryLayer;
        std::unique_ptr<X11WindowedEglCursorLayer> cursorLayer;
    };

    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, Layers> m_outputs;
    X11WindowedBackend *m_backend;
};

} // namespace KWin
