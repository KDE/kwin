/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "opengl/eglbackend.h"
#include "opengl/eglnativefence.h"
#include "utils/damagejournal.h"
#include "wayland_layer.h"

#include <memory>

struct wl_buffer;

namespace KWin
{
class EglSwapchainSlot;
class EglSwapchain;
class GLFramebuffer;
class GraphicsBufferAllocator;
class GLRenderTimeQuery;

namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class WaylandEglBackend;

class WaylandEglLayer : public WaylandLayer
{
public:
    WaylandEglLayer(WaylandOutput *output, WaylandEglBackend *backend, OutputLayerType type, int zpos);
    ~WaylandEglLayer() override;

    GLFramebuffer *fbo() const;
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    DamageJournal m_damageJournal;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_buffer;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    WaylandEglBackend *const m_backend;

    friend class WaylandEglBackend;
};

class WaylandEglCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend);
    ~WaylandEglCursorLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    WaylandEglBackend *m_backend;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_buffer;
    std::unique_ptr<GLRenderTimeQuery> m_query;
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
class WaylandEglBackend : public EglBackend
{
    Q_OBJECT
public:
    WaylandEglBackend(WaylandBackend *b);
    ~WaylandEglBackend() override;

    WaylandBackend *backend() const;
    DrmDevice *drmDevice() const override;

    void init() override;
    QList<OutputLayer *> compatibleOutputLayers(Output *output) override;

private:
    bool initializeEgl();
    bool initRenderingContext();
    void createOutputLayers(Output *output);
    void cleanupSurfaces() override;

    WaylandBackend *m_backend;
};

} // namespace Wayland
} // namespace KWin
