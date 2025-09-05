/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "opengl/eglbackend.h"
#include "opengl/glutils.h"

namespace KWin
{

class EglSwapchainSlot;
class EglSwapchain;
class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;
class GLRenderTimeQuery;

class X11WindowedEglPrimaryLayer : public OutputLayer
{
public:
    X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output);
    ~X11WindowedEglPrimaryLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

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

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

private:
    X11WindowedEglBackend *const m_backend;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLRenderTimeQuery> m_query;
};

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class X11WindowedEglBackend : public EglBackend
{
    Q_OBJECT

public:
    explicit X11WindowedEglBackend(X11WindowedBackend *backend);
    ~X11WindowedEglBackend() override;

    X11WindowedBackend *backend() const;
    DrmDevice *drmDevice() const override;

    void init() override;
    void endFrame(Output *output, const QRegion &renderedRegion, const QRegion &damagedRegion);
    QList<OutputLayer *> compatibleOutputLayers(Output *output) override;

private:
    bool initializeEgl();
    bool initRenderingContext();

    X11WindowedBackend *m_backend;
};

} // namespace KWin
