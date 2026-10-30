/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_layer.h"
#include "core/renderbackend.h"
#include "core/renderdevice.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_virtual_output.h"
#include "swapchain.h"
#include "utils/envvar.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <unistd.h>

namespace KWin
{

VirtualDrmLayer::VirtualDrmLayer(RenderBackend *backend, DrmVirtualOutput *output)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_backend(backend)
{
}

VirtualDrmLayer::~VirtualDrmLayer()
{
    releaseBuffers();
}

std::optional<OutputLayerBeginFrameInfo> VirtualDrmLayer::beginFrame(OutputFrame *frame)
{
    m_scanoutBuffer.reset();

    if (doesSwapchainFit(m_swapchain.get())) {
        m_oldSwapchain.reset();
        m_oldDamageJournal.clear();
    } else {
        if (doesSwapchainFit(m_oldSwapchain.get())) {
            m_swapchain = m_oldSwapchain;
            m_damageJournal = m_oldDamageJournal;
        } else {
            if (const auto swapchain = createSwapchain()) {
                m_oldSwapchain = m_swapchain;
                m_oldDamageJournal = m_damageJournal;
                m_swapchain = swapchain;
                m_damageJournal = DamageJournal();
            } else {
                return std::nullopt;
            }
        }
    }

    auto slot = m_swapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }
    m_currentSlot = slot;

    const Region repair = m_damageJournal.accumulate(slot->age(), Region::infinite());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentSlot->buffer(), OutputTransform::Normal),
        .repaint = repair,
    };
}

bool VirtualDrmLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    m_damageJournal.add(damagedDeviceRegion);
    m_swapchain->release(m_currentSlot.get(), std::move(sync));
    return true;
}

std::shared_ptr<Swapchain> VirtualDrmLayer::createSwapchain() const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    const bool allowModifiers = !modifiersEnvSet || modifiersEnv;

    const auto allFormats = m_backend->supportedFormats();
    const auto formats = OutputLayer::filterAndSortFormats(allFormats, 0, m_output->colorPowerTradeoff());
    for (const auto &format : formats) {
        const auto size = m_output->modeSize();

        GraphicsBufferOptions options{
            .size = size,
            .format = format.drmFormat,
            .modifiers = allowModifiers ? allFormats[format.drmFormat] : ModifierList{DRM_FORMAT_MOD_INVALID},
            .software = false,
            .scanout = false,
        };
        if (auto swapchain = Swapchain::create(m_backend->renderDevice()->allocator(), options)) {
            return swapchain;
        }
    }
    qCWarning(KWIN_DRM) << "couldn't create a swapchain for a virtual output!";
    return nullptr;
}

bool VirtualDrmLayer::doesSwapchainFit(Swapchain *swapchain) const
{
    return swapchain && swapchain->size() == m_output->modeSize();
}

bool VirtualDrmLayer::earlyScanoutChecks()
{
    return true;
}

bool VirtualDrmLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    m_scanoutBuffer = buffer;
    return true;
}

void VirtualDrmLayer::releaseBuffers()
{
    m_swapchain.reset();
    m_oldSwapchain.reset();
    m_currentSlot.reset();
    m_scanoutBuffer.reset();
}

FormatModifierMap VirtualDrmLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

}
