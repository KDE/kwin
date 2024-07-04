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
    , QtWaylandServer::wp_presentation(*display, 2)
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

    auto &feedback = surfPriv->pending->presentationFeedback;
    if (!feedback) {
        feedback = std::make_unique<PresentationTimeFeedback>();
    }

    wl_resource *feedbackResource = wl_resource_create(resource->client(), &wp_presentation_feedback_interface, resource->version(), callback);
    wl_resource_set_implementation(feedbackResource, nullptr, nullptr, [](wl_resource *resource) {
        wl_list_remove(wl_resource_get_link(resource));
    });

    wl_list_insert(feedback->resources.prev, wl_resource_get_link(feedbackResource));
}

PresentationTimeFeedback::PresentationTimeFeedback()
{
    wl_list_init(&resources);
}

PresentationTimeFeedback::~PresentationTimeFeedback()
{
    wl_resource *resource;
    wl_resource *tmp;
    wl_resource_for_each_safe (resource, tmp, &resources) {
        wp_presentation_feedback_send_discarded(resource);
        wl_resource_destroy(resource);
    }
}

void PresentationTimeFeedback::presented(std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timestamp);
    const uint32_t tvSecHi = secs.count() >> 32;
    const uint32_t tvSecLo = secs.count() & 0xffffffff;
    const uint32_t tvNsec = (timestamp - secs).count();

    const bool adaptiveSync = mode == PresentationMode::AdaptiveSync || mode == PresentationMode::AdaptiveAsync;
    uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK | WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;
    if (mode == PresentationMode::VSync || mode == PresentationMode::AdaptiveSync) {
        flags |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
    }

    wl_resource *resource;
    wl_resource *tmp;
    wl_resource_for_each_safe (resource, tmp, &resources) {
        // TODO with adaptive sync, send an estimation of the current actual refresh rate?
        uint32_t refreshDuration = refreshCycleDuration.count();
        if (adaptiveSync && wl_resource_get_version(resource) == 1) {
            // version 1 requires sending zero when the refresh rate isn't stable
            refreshDuration = 0;
        }
        wp_presentation_feedback_send_presented(resource, tvSecHi, tvSecLo, tvNsec, refreshDuration, 0, 0, flags);
        wl_resource_destroy(resource);
    }
}
}
