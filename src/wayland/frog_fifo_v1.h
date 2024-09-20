#pragma once
#include <QObject>
#include <QPointer>

#include "wayland/qwayland-server-frog-fifo-v1.h"

namespace KWin
{

class Display;
class SurfaceInterface;
class Transaction;

class FrogFifoManagerV1 : public QObject, public QtWaylandServer::frog_fifo_manager_v1
{
    Q_OBJECT
public:
    explicit FrogFifoManagerV1(Display *display, QObject *parent);

private:
    void frog_fifo_manager_v1_destroy(Resource *resource) override;
    void frog_fifo_manager_v1_get_fifo(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class XXFifoV1Surface : public QtWaylandServer::frog_fifo_surface_v1
{
public:
    explicit XXFifoV1Surface(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~XXFifoV1Surface();

private:
    void frog_fifo_surface_v1_destroy_resource(Resource *resource) override;
    void frog_fifo_surface_v1_destroy(Resource *resource) override;
    void frog_fifo_surface_v1_set_barrier(Resource *resource) override;
    void frog_fifo_surface_v1_wait_barrier(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
