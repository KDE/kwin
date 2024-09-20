#pragma once
#include <QObject>
#include <QPointer>

#include "wayland/qwayland-server-fifo-v1.h"

namespace KWin
{

class Display;
class SurfaceInterface;
class Transaction;

class FifoManagerV1 : public QObject, public QtWaylandServer::wp_fifo_manager_v1
{
    Q_OBJECT
public:
    explicit FifoManagerV1(Display *display, QObject *parent);

private:
    void wp_fifo_manager_v1_destroy(Resource *resource) override;
    void wp_fifo_manager_v1_get_fifo(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class FifoV1Surface : public QtWaylandServer::wp_fifo_v1
{
public:
    explicit FifoV1Surface(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~FifoV1Surface();

private:
    void wp_fifo_v1_destroy_resource(Resource *resource) override;
    void wp_fifo_v1_destroy(Resource *resource) override;
    void wp_fifo_v1_set_barrier(Resource *resource) override;
    void wp_fifo_v1_wait_barrier(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
