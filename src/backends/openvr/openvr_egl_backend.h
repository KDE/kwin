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
class OpenVRBackend;
class GLFramebuffer;
class GLTexture;
class OpenVREglBackend;
class GLRenderTimeQuery;

class KWIN_EXPORT OpenVREglLayer : public OutputLayer
{
public:
    OpenVREglLayer(BackendOutput *output, OpenVREglBackend *backend);
    ~OpenVREglLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &renderedDeviceRegion, const Region &damagedDeviceRegion, OutputFrame *frame) override;

    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

    GLTexture *texture() const;

private:
    OpenVREglBackend *const m_backend;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_current;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    DamageJournal m_damageJournal;
};

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class OpenVREglBackend : public EglBackend
{
    Q_OBJECT

public:
    OpenVREglBackend(OpenVRBackend *b, RenderDevice *renderDevice);
    ~OpenVREglBackend() override;

    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;
    bool init() override;

    OpenVRBackend *backend() const;

private:
    void addOutput(BackendOutput *output);

    OpenVRBackend *m_backend;
};

} // namespace KWin
