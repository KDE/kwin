/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "opengl/eglbackend.h"
#include "utils/damagejournal.h"

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

class KWIN_EXPORT VirtualEglLayer : public OutputLayer
{
public:
    VirtualEglLayer(Output *output, VirtualEglBackend *backend);
    ~VirtualEglLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    void releaseBuffers() override;

    GLTexture *texture() const;

private:
    VirtualEglBackend *const m_backend;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_current;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    DamageJournal m_damageJournal;
};

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class VirtualEglBackend : public EglBackend
{
    Q_OBJECT

public:
    VirtualEglBackend(VirtualBackend *b);
    ~VirtualEglBackend() override;
    QList<OutputLayer *> compatibleOutputLayers(Output *output) override;
    void init() override;

    VirtualBackend *backend() const;
    DrmDevice *drmDevice() const override;

private:
    bool initializeEgl();
    bool initRenderingContext();

    void addOutput(Output *output);

    VirtualBackend *m_backend;
};

} // namespace KWin
