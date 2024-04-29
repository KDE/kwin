/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "drm_layer.h"
#include "utils/damagejournal.h"

#include <QMap>
#include <QPointer>
#include <QRegion>
#include <epoxy/egl.h>
#include <optional>

namespace KWin
{

class EglSwapchain;
class EglSwapchainSlot;
class GLTexture;
class EglGbmBackend;
class DrmVirtualOutput;
class GLRenderTimeQuery;
class SurfaceInterface;

class VirtualEglGbmLayer : public DrmOutputLayer
{
public:
    VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output);
    ~VirtualEglGbmLayer() override;

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    std::shared_ptr<GLTexture> texture() const override;
    void releaseBuffers() override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    const ColorDescription &colorDescription() const;

private:
    bool doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color) override;
    std::shared_ptr<EglSwapchain> createGbmSwapchain() const;
    bool doesGbmSwapchainFit(EglSwapchain *swapchain) const;

    GraphicsBufferRef m_scanoutBuffer;
    ColorDescription m_scanoutColor = ColorDescription::sRGB;
    DamageJournal m_damageJournal;
    DamageJournal m_oldDamageJournal;
    std::shared_ptr<EglSwapchain> m_gbmSwapchain;
    std::shared_ptr<EglSwapchain> m_oldGbmSwapchain;
    std::shared_ptr<EglSwapchainSlot> m_currentSlot;
    std::unique_ptr<GLRenderTimeQuery> m_query;

    EglGbmBackend *const m_eglBackend;
};

}
