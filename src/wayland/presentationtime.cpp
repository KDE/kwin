/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "presentationtime.h"
#include "display.h"
#include "output.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

PresentationTime::PresentationTime(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_presentation(*display, 1)
{
}

void PresentationTime::wp_presentation_bind_resource(Resource *resource)
{
    send_clock_id(resource->handle, CLOCK_MONOTONIC);
}

void PresentationTime::wp_presentation_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PresentationTime::wp_presentation_feedback(Resource *resource, wl_resource *surface, uint32_t callback)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(surf);
    surfPriv->pending->presentationFeedbacks.push_back(std::make_unique<PresentationTimeFeedback>(surf, resource->client(), callback));
}

PresentationTimeFeedback::PresentationTimeFeedback(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : QtWaylandServer::wp_presentation_feedback(client, id, 1)
    , m_surface(surface)
{
}

PresentationTimeFeedback::~PresentationTimeFeedback()
{
    if (!m_presented) {
        send_discarded();
    }
}

void PresentationTimeFeedback::presented(uint64_t msc, std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timestamp);
    const uint32_t tvSecHi = secs.count() >> 32;
    const uint32_t tvSecLo = secs.count() & 0xffffffff;
    const uint32_t tvNsec = (timestamp - secs).count();

    const bool adaptiveSync = mode == PresentationMode::AdaptiveSync || mode == PresentationMode::AdaptiveAsync;
    const uint32_t refreshDuration = adaptiveSync ? 0 : refreshCycleDuration.count();
    uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK | WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;
    if (mode == PresentationMode::VSync || mode == PresentationMode::AdaptiveSync) {
        flags |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
    }

    send_presented(resource()->handle, tvSecHi, tvSecLo, tvNsec, refreshDuration, msc >> 32, msc & 0xFFFFFFFF, flags);
    m_presented = true;
}
}
