/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_layer.h"
#include "color_manager.h"
#include "core/renderbackend.h"
#include "core/renderdevice.h"
#include "swapchain.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland-client/viewporter.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include "wayland-presentation-time-client-protocol.h"
#include "wayland-tearing-control-v1-client-protocol.h"

namespace KWin
{
namespace Wayland
{

WaylandLayer::WaylandLayer(WaylandOutput *output, RenderBackend *backend, WaylandDisplay *display, OutputLayerType type, int zpos)
    : OutputLayer(output, type, zpos, zpos, zpos)
    , m_backend(backend)
    , m_display(display)
    , m_surface(m_display->compositor()->createSurface())
    , m_subSurface(m_display->subCompositor()->createSubSurface(m_surface.get(), output->surface()))
{
    // make presentation atomic with WaylandOutput::present
    m_subSurface->setMode(KWayland::Client::SubSurface::Mode::Synchronized);
    if (auto manager = display->tearingControl()) {
        m_tearingControl = wp_tearing_control_manager_v1_get_tearing_control(manager, *m_surface);
    }
    if (auto manager = display->colorManager()) {
        const bool supportsMinFeatures = manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)
            && manager->supportsTransferFunction(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
        if (supportsMinFeatures) {
            m_colorSurface = wp_color_manager_v1_get_surface(manager->object(), *m_surface);
        }
    }
    m_viewport = display->viewporter()->createViewport(*m_surface);
}

WaylandLayer::~WaylandLayer()
{
    if (m_tearingControl) {
        wp_tearing_control_v1_destroy(m_tearingControl);
    }
    if (m_colorSurface) {
        wp_color_management_surface_v1_destroy(m_colorSurface);
    }
}

bool WaylandLayer::test() const
{
    if (!isEnabled()) {
        return true;
    }
    if (!m_colorSurface && m_color != ColorDescription::sRGB) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal) {
        // TODO allow this?
        return false;
    }
    if (sourceRect().isEmpty()) {
        return false;
    }
    return true;
}

void WaylandLayer::setBuffer(GraphicsBuffer *buffer, const Region &deviceDamagedRegion)
{
    m_pendingBuffer = buffer;
    m_pendingDamage = deviceDamagedRegion;
}

void WaylandLayer::commit(PresentationMode presentationMode)
{
    if (!isEnabled()) {
        m_pendingBuffer.reset();
        m_surface->attachBuffer((wl_buffer *)nullptr);
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
        return;
    }
    WaylandOutput *output = static_cast<WaylandOutput *>(m_output.get());
    // this is a bit annoying, we need a new Wayland protocol
    // to do this properly with fractional scaling. Until we
    // have that, it may cause blurriness in some cases!
    const Rect logicalTarget = targetRect().scaled(1.0 / m_output->scale()).rounded();
    if (m_viewport) {
        m_viewport->setSource(sourceRect());
        m_viewport->setDestination(logicalTarget.size());
    }
    m_subSurface->setPosition(logicalTarget.topLeft());
    if (m_colorSurface && m_color != m_previousColor) {
        const auto imageDescription = m_display->colorManager()->createImageDescription(*m_color);
        wp_color_management_surface_v1_set_image_description(m_colorSurface, imageDescription, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
        wp_image_description_v1_destroy(imageDescription);
        m_previousColor = m_color;
    }
    if (m_tearingControl) {
        if (presentationMode == PresentationMode::Async) {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);
        } else {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
        }
    }
    if (m_pendingBuffer) {
        m_surface->attachBuffer(output->backend()->importBuffer(m_pendingBuffer.get()));
        m_surface->damageBuffer(QRegion(m_pendingDamage));
        // WaylandBackend::importBuffer takes care of the buffers life time from here on
        m_pendingBuffer.reset();
    }
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

KWayland::Client::Surface *WaylandLayer::surface() const
{
    return m_surface.get();
}

KWayland::Client::SubSurface *WaylandLayer::subSurface() const
{
    return m_subSurface.get();
}

std::optional<OutputLayerBeginFrameInfo> WaylandLayer::beginFrame(OutputFrame *frame)
{
    if (m_color != m_previousColor) {
        // need to force a full repaint
        m_damageJournal.clear();
    }

    const QSize nativeSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const FormatModifierMap formatTable = m_display->linuxDmabuf()->formats().intersected(m_backend->supportedFormats());
        const auto suitableFormats = filterAndSortFormats(formatTable, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        for (const auto &candidate : suitableFormats) {
            auto it = formatTable.constFind(candidate.drmFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            GraphicsBufferOptions options{
                .size = nativeSize,
                .format = it.key(),
                .modifiers = it.value(),
                .software = false,
                .scanout = false,
            };
            m_swapchain = Swapchain::create(m_backend->renderDevice()->allocator(), options);
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
    }

    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        return std::nullopt;
    }

    // The picture is composed for the display as the user sees it, while the buffer has the
    // size the panel is driven at, so a rotated display has to be turned on the way in.
    const OutputTransform contentTransform = bufferTransform().combine(OutputTransform::FlipY);
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentSlot->buffer(), contentTransform, m_color),
        .repaint = m_damageJournal.accumulate(m_currentSlot->age(), Region::infinite()),
    };
}

bool WaylandLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    // The damage is where the picture changed, and the picture is composed the way the user
    // sees it. What the host reads is the buffer, which a rotated display fills turned, so
    // the region has to be turned with it - the journal stays in the picture's own
    // coordinates, because that is what it is compared against next time.
    const auto mapping = bufferTransform();
    const QSize pictureSize = mapping.map(m_swapchain->size());
    setBuffer(m_currentSlot->buffer(), mapping.map(damagedDeviceRegion & Rect(QPoint(), pictureSize), pictureSize));
    m_swapchain->release(m_currentSlot.get(), std::move(sync));

    m_damageJournal.add(damagedDeviceRegion);
    return true;
}

bool WaylandLayer::earlyScanoutChecks()
{
    return test();
}

bool WaylandLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    setBuffer(buffer, Region::infinite());
    return true;
}

FormatModifierMap WaylandLayer::supportedDrmFormats() const
{
    return m_display->linuxDmabuf()->formats();
}

void WaylandLayer::releaseBuffers()
{
    m_currentSlot.reset();
    m_swapchain.reset();
}

WaylandCursorLayer::WaylandCursorLayer(WaylandOutput *output, RenderBackend *backend, WaylandDisplay *display)
    : OutputLayer(output, OutputLayerType::CursorOnly, 255, 255, 255)
    , m_outputBackend(output->backend())
    , m_renderBackend(backend)
    , m_display(display)
{
}

WaylandCursorLayer::~WaylandCursorLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandCursorLayer::beginFrame(OutputFrame *frame)
{
    const auto bufferSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const FormatModifierMap formatTable = m_display->linuxDmabuf()->formats().intersected(m_renderBackend->supportedFormats());
        const auto suitableFormats = filterAndSortFormats(formatTable, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        for (const auto &candidate : suitableFormats) {
            auto it = formatTable.constFind(candidate.drmFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            GraphicsBufferOptions options{
                .size = bufferSize,
                .format = it.key(),
                .modifiers = it.value(),
                .software = false,
                .scanout = false,
            };
            m_swapchain = Swapchain::create(m_renderBackend->renderDevice()->allocator(), options);
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->buffer(), bufferTransform().combine(OutputTransform::FlipY)),
        .repaint = Region::infinite(),
    };
}

bool WaylandCursorLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    wl_buffer *buffer = m_outputBackend->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    static_cast<WaylandOutput *>(m_output.get())->cursor()->update(buffer, m_buffer->buffer()->size() / m_output->scale(), (hotspot() / m_output->scale()).toPoint());

    m_swapchain->release(m_buffer.get(), std::move(sync));
    return true;
}

FormatModifierMap WaylandCursorLayer::supportedDrmFormats() const
{
    return m_renderBackend->supportedFormats();
}

void WaylandCursorLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

}
}
