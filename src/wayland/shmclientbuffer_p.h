/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "wayland/shmclientbuffer.h"
#include "utils/filedescriptor.h"
#include "utils/memorymap.h"

#include "qwayland-server-wayland.h"

namespace KWin
{

class ShmClientBufferIntegrationPrivate : public QtWaylandServer::wl_shm
{
public:
    ShmClientBufferIntegrationPrivate(Display *display, ShmClientBufferIntegration *q);

    ShmClientBufferIntegration *q;

protected:
    void shm_bind_resource(Resource *resource) override;
    void shm_create_pool(Resource *resource, uint32_t id, int32_t fd, int32_t size) override;
};

class ShmPool : public QtWaylandServer::wl_shm_pool
{
public:
    ShmPool(ShmClientBufferIntegration *integration, wl_client *client, int id, uint32_t version, FileDescriptor &&fd, std::shared_ptr<MemoryMap> mapping);

    void ref();
    void unref();

    ShmClientBufferIntegration *integration;
    std::shared_ptr<MemoryMap> mapping;
    FileDescriptor fd;
    int refCount = 1;
    bool sigbusImpossible = false;

protected:
    void shm_pool_destroy_resource(Resource *resource) override;
    void shm_pool_create_buffer(Resource *resource, uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) override;
    void shm_pool_destroy(Resource *resource) override;
    void shm_pool_resize(Resource *resource, int32_t size) override;
};

struct ShmAccess
{
    std::shared_ptr<MemoryMap> mapping;
    int count = 0;
    std::atomic<ShmAccess *> next = nullptr;
};

class KWIN_EXPORT ShmClientBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    ShmClientBuffer(ShmPool *pool, ShmAttributes attributes, wl_client *client, uint32_t id);
    ~ShmClientBuffer() override;

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const ShmAttributes *shmAttributes() const override;

    static ShmClientBuffer *get(wl_resource *resource);

private:
    static void buffer_destroy_resource(wl_resource *resource);
    static void buffer_destroy(wl_client *client, wl_resource *resource);
    static const struct wl_buffer_interface implementation;

    wl_resource *m_resource = nullptr;
    ShmPool *m_shmPool;
    ShmAttributes m_shmAttributes;
    std::optional<ShmAccess> m_shmAccess;
};

} // namespace KWin
