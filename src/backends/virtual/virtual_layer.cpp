/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_layer.h"
#include "core/renderbackend.h"
#include "core/renderdevice.h"
#include "swapchain.h"

namespace KWin
{

VirtualLayer::VirtualLayer(BackendOutput *output, RenderBackend *backend)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_backend(backend)
{
}

VirtualLayer::~VirtualLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> VirtualLayer::beginFrame(OutputFrame *frame)
{
    const QSize nativeSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        GraphicsBufferOptions options{
            .size = nativeSize,
            .format = DRM_FORMAT_XRGB8888,
            .modifiers = m_backend->supportedFormats()[DRM_FORMAT_XRGB8888],
            .software = false,
            .scanout = false,
        };
        m_swapchain = Swapchain::create(m_backend->renderDevice()->allocator(), options);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->buffer(), bufferTransform().combine(OutputTransform::FlipY)),
        .repaint = m_damageJournal.accumulate(m_current->age(), Region::infinite()),
    };
}

bool VirtualLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    m_swapchain->release(m_current.get(), std::move(sync));
    m_damageJournal.add(damagedDeviceRegion);
    return true;
}

FormatModifierMap VirtualLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void VirtualLayer::releaseBuffers()
{
    m_current.reset();
    m_swapchain.reset();
}

QImage VirtualLayer::download() const
{
    if (!m_current) {
        return QImage{};
    }
    return m_backend->renderDevice()->download(m_current->buffer());
}

}
