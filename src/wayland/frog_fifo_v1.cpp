#include "frog_fifo_v1.h"

#include "display.h"
#include "surface_p.h"
#include "transaction_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

FrogFifoManagerV1::FrogFifoManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::frog_fifo_manager_v1(*display, s_version)
{
}

void FrogFifoManagerV1::frog_fifo_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FrogFifoManagerV1::frog_fifo_manager_v1_get_fifo(Resource *resource, uint32_t id, struct ::wl_resource *wlSurface)
{
    const auto surface = SurfaceInterface::get(wlSurface);
    const auto surfacePrivate = SurfaceInterfacePrivate::get(surface);
    if (surfacePrivate->fifoSurface) {
        wl_resource_post_error(resource->handle, error_fifo_surface_already_exists, "Attempted to create a second fifo surface for the wl_surface");
        return;
    }
    surfacePrivate->fifoSurface = new XXFifoV1Surface(resource->client(), id, resource->version(), surface);
}

XXFifoV1Surface::XXFifoV1Surface(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::frog_fifo_surface_v1(client, id, version)
    , m_surface(surface)
{
}

XXFifoV1Surface::~XXFifoV1Surface()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->fifoSurface = nullptr;
    }
}

void XXFifoV1Surface::frog_fifo_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XXFifoV1Surface::frog_fifo_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXFifoV1Surface::frog_fifo_surface_v1_set_barrier(Resource *resource)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, error_surface_destroyed, "called set_barrier on a destroyed surface");
        return;
    }
    SurfaceInterfacePrivate::get(m_surface)->pending->fifoBarrier = std::make_unique<FifoBarrier>();
}

void XXFifoV1Surface::frog_fifo_surface_v1_wait_barrier(Resource *resource)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, error_surface_destroyed, "called wait_barrier on a destroyed surface");
        return;
    }
    SurfaceInterfacePrivate::get(m_surface)->pending->hasFifoWaitCondition = true;
}

}
