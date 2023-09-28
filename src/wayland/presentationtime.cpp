/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/presentationtime.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "wayland/display.h"
#include "wayland/output.h"
#include "wayland/surface_p.h"

#include "wayland/qwayland-server-presentation-time.h"

namespace KWin
{

static const quint32 s_version = 1;

class PresentationInterfacePrivate : public QtWaylandServer::wp_presentation
{
public:
    explicit PresentationInterfacePrivate(Display *display);

    clock_t clockId = 0;

protected:
    void wp_presentation_bind_resource(Resource *resource) override;
    void wp_presentation_destroy(Resource *resource) override;
    void wp_presentation_feedback(Resource *resource, ::wl_resource *surface_resource, uint32_t callback) override;
};

PresentationInterfacePrivate::PresentationInterfacePrivate(Display *display)
    : QtWaylandServer::wp_presentation(*display, s_version)
{
}

void PresentationInterfacePrivate::wp_presentation_bind_resource(Resource *resource)
{
    send_clock_id(resource->handle, clockId);
}

void PresentationInterfacePrivate::wp_presentation_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PresentationInterfacePrivate::wp_presentation_feedback(Resource *resource, ::wl_resource *surface_resource, uint32_t callback)
{
    auto surface = SurfaceInterface::get(surface_resource);
    auto surfacePrivate = SurfaceInterfacePrivate::get(surface);

    QPointer<PresentationFeedback> &feedback = surfacePrivate->pending->presentationFeedback;
    if (!feedback) {
        feedback = new PresentationFeedback(surface);
    }

    wl_resource *feedbackResource = wl_resource_create(resource->client(), &wp_presentation_feedback_interface, resource->version(), callback);
    wl_resource_set_implementation(feedbackResource, nullptr, nullptr, [](wl_resource *resource) {
        wl_list_remove(wl_resource_get_link(resource));
    });

    wl_list_insert(feedback->m_resources.prev, wl_resource_get_link(feedbackResource));
}

PresentationInterface::PresentationInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<PresentationInterfacePrivate>(display))
{
}

PresentationInterface::~PresentationInterface()
{
}

clock_t PresentationInterface::clockId() const
{
    return d->clockId;
}

void PresentationInterface::setClockId(clock_t clockId)
{
    d->clockId = clockId;
}

PresentationFeedback *PresentationInterface::surfaceCopied(SurfaceInterface *surface, OutputInterface *output)
{
    PresentationFeedback *feedback = std::exchange(SurfaceInterfacePrivate::get(surface)->current->presentationFeedback, nullptr);
    if (!feedback) {
        return nullptr;
    }

    connect(output, &OutputInterface::removed, feedback, [feedback]() {
        delete feedback;
    });

    feedback->schedulePresented(output);
    return feedback;
}

PresentationFeedback *PresentationInterface::surfaceScannedOut(SurfaceInterface *surface, OutputInterface *output)
{
    PresentationFeedback *feedback = std::exchange(SurfaceInterfacePrivate::get(surface)->current->presentationFeedback, nullptr);
    if (!feedback) {
        return nullptr;
    }

    connect(output, &OutputInterface::removed, feedback, [feedback]() {
        delete feedback;
    });

    feedback->schedulePresented(output);
    return feedback;
}

PresentationFeedback::PresentationFeedback(SurfaceInterface *surface)
    : QObject(surface)
    , m_surface(surface)
{
    wl_list_init(&m_resources);
}

PresentationFeedback::~PresentationFeedback()
{
    sendDiscarded();
}

void PresentationFeedback::sendDiscarded()
{
    wl_resource *resource;
    wl_resource *tmp;
    wl_resource_for_each_safe (resource, tmp, &m_resources) {
        wp_presentation_feedback_send_discarded(resource);
        wl_resource_destroy(resource);
    }
}

void PresentationFeedback::sendPresented(std::chrono::nanoseconds timestamp, std::chrono::nanoseconds refresh, uint64_t sequence, uint32_t flags)
{
    const auto outputs = m_output->clientResources(m_surface->client());

    const std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(timestamp);
    const std::chrono::nanoseconds nanoseconds = timestamp - seconds;
    const uint32_t secHi = seconds.count() >> 32;
    const uint32_t secLo = seconds.count() & 0xFFFFFFFF;
    const uint32_t seqHi = sequence >> 32;
    const uint32_t seqLo = sequence & 0xFFFFFFFF;

    wl_resource *resource;
    wl_resource *tmp;
    wl_resource_for_each_safe (resource, tmp, &m_resources) {
        for (const auto &output : outputs) {
            wp_presentation_feedback_send_sync_output(resource, output);
        }
        wp_presentation_feedback_send_presented(resource, secHi, secLo, nanoseconds.count(), refresh.count(), seqHi, seqLo, flags);
        wl_resource_destroy(resource);
    }
}

void PresentationFeedback::schedulePresented(OutputInterface *output)
{
    m_output = output;

    RenderLoop *loop = output->handle()->renderLoop();
    uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC | WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK | WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION; // FIXME: we are must be told this information

    connect(loop, &RenderLoop::framePresented, this, [this, flags](RenderLoop *loop, std::chrono::nanoseconds timestamp) {
        sendPresented(timestamp, std::chrono::nanoseconds(1'000'000'000'000ull / loop->refreshRate()), 0, flags);
        delete this;
    });
}

} // namespace KWin

#include "moc_presentationtime.cpp"
