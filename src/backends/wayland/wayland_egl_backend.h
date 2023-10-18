/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_egl_backend.h"
#include "core/outputlayer.h"
#include "utils/damagejournal.h"

#include <KWayland/Client/buffer.h>

#include <memory>

class QTemporaryFile;
struct wl_buffer;
struct wl_shm;
struct gbm_bo;

namespace KWin
{
class GLFramebuffer;

namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class WaylandEglBackend;

class WaylandEglLayerBuffer
{
public:
    WaylandEglLayerBuffer(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, WaylandEglBackend *backend);
    ~WaylandEglLayerBuffer();

    wl_buffer *buffer() const;
    GLFramebuffer *framebuffer() const;
    GLFramebuffer *shadowFramebuffer() const;
    GLTexture *shadowTexture() const;
    int age() const;

private:
    WaylandEglBackend *m_backend;
    wl_buffer *m_buffer = nullptr;
    gbm_bo *m_bo = nullptr;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::unique_ptr<GLFramebuffer> m_shadowFramebuffer;
    std::shared_ptr<GLTexture> m_texture;
    std::shared_ptr<GLTexture> m_shadowTexture;
    int m_age = 0;
    friend class WaylandEglLayerSwapchain;
};

class WaylandEglLayerSwapchain
{
public:
    WaylandEglLayerSwapchain(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, WaylandEglBackend *backend);
    ~WaylandEglLayerSwapchain();

    QSize size() const;

    std::shared_ptr<WaylandEglLayerBuffer> acquire();
    void release(std::shared_ptr<WaylandEglLayerBuffer> buffer);

private:
    WaylandEglBackend *m_backend;
    QSize m_size;
    QVector<std::shared_ptr<WaylandEglLayerBuffer>> m_buffers;
    int m_index = 0;
};

class WaylandEglPrimaryLayer : public OutputLayer
{
public:
    WaylandEglPrimaryLayer(WaylandOutput *output, WaylandEglBackend *backend);
    ~WaylandEglPrimaryLayer() override;

    GLFramebuffer *fbo() const;
    void present();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    WaylandOutput *m_waylandOutput;
    DamageJournal m_damageJournal;
    std::unique_ptr<WaylandEglLayerSwapchain> m_swapchain;
    std::shared_ptr<WaylandEglLayerBuffer> m_buffer;
    WaylandEglBackend *const m_backend;

    friend class WaylandEglBackend;
};

class WaylandEglCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend);
    ~WaylandEglCursorLayer() override;

    qreal scale() const;
    void setScale(qreal scale);

    QPoint hotspot() const;
    void setHotspot(const QPoint &hotspot);

    QSize size() const;
    void setSize(const QSize &size);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    WaylandOutput *m_output;
    WaylandEglBackend *m_backend;
    std::unique_ptr<WaylandEglLayerSwapchain> m_swapchain;
    std::shared_ptr<WaylandEglLayerBuffer> m_buffer;
    QPoint m_hotspot;
    QSize m_size;
    qreal m_scale = 1.0;
};

/**
 * @brief OpenGL Backend using Egl on a Wayland surface.
 *
 * This Backend is the basis for a session compositor running on top of a Wayland system compositor.
 * It creates a Surface as large as the screen and maps it as a fullscreen shell surface on the
 * system compositor. The OpenGL context is created on the Wayland surface, so for rendering X11 is
 * not involved.
 *
 * Also in repainting the backend is currently still rather limited. Only supported mode is fullscreen
 * repaints, which is obviously not optimal. Best solution is probably to go for buffer_age extension
 * and make it the only available solution next to fullscreen repaints.
 */
class WaylandEglBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    WaylandEglBackend(WaylandBackend *b);
    ~WaylandEglBackend() override;

    WaylandBackend *backend() const;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    void init() override;
    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    WaylandEglCursorLayer *cursorLayer(Output *output);

    std::shared_ptr<KWin::GLTexture> textureForOutput(KWin::Output *output) const override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    bool createEglWaylandOutput(Output *output);
    void cleanupSurfaces() override;

    struct Layers
    {
        std::unique_ptr<WaylandEglPrimaryLayer> primaryLayer;
        std::unique_ptr<WaylandEglCursorLayer> cursorLayer;
    };

    WaylandBackend *m_backend;
    std::map<Output *, Layers> m_outputs;
    bool m_havePlatformBase;
};

} // namespace Wayland
} // namespace KWin
