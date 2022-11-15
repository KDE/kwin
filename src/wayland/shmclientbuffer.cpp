/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "shmclientbuffer.h"
#include "clientbuffer_p.h"
#include "display.h"
#include "display_p.h"
#include "utils/filedescriptor.h"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>

#include "qwayland-server-wayland.h"

using namespace KWin;

namespace KWaylandServer
{

static constexpr int s_version = 1;

static pthread_once_t sigbusOnce = PTHREAD_ONCE_INIT;
static pthread_key_t sigbusDataKey;
static struct sigaction prevSigbusAction;

class ShmClientBufferIntegrationPrivate : public QtWaylandServer::wl_shm
{
public:
    ShmClientBufferIntegrationPrivate(Display *display, ShmClientBufferIntegration *q);

    ShmClientBufferIntegration *q;
    QVector<uint32_t> formats;

protected:
    void shm_bind_resource(Resource *resource) override;
    void shm_create_pool(Resource *resource, uint32_t id, int32_t fd, int32_t size) override;
};

class ShmMapping
{
public:
    ShmMapping(int fd, int32_t size);
    ~ShmMapping();

    bool isValid() const;

    void *data;
    int32_t size;
};

class ShmSigbusData
{
public:
    std::shared_ptr<ShmMapping> mapping;
    int accessCount = 0;
};

class ShmPool : public QtWaylandServer::wl_shm_pool
{
public:
    ShmPool(ShmClientBufferIntegration *integration, wl_client *client, int id, uint32_t version, FileDescriptor &&fd, std::shared_ptr<ShmMapping> mapping);
    ~ShmPool() override;

    void ref();
    void unref();

    ShmClientBufferIntegration *integration;
    std::shared_ptr<ShmMapping> mapping;
    FileDescriptor fd;
    int refCount = 1;

protected:
    void shm_pool_destroy_resource(Resource *resource) override;
    void shm_pool_create_buffer(Resource *resource, uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) override;
    void shm_pool_destroy(Resource *resource) override;
    void shm_pool_resize(Resource *resource, int32_t size) override;
};

class ShmClientBufferPrivate : public ClientBufferPrivate, public QtWaylandServer::wl_buffer
{
public:
    ShmClientBufferPrivate(ShmClientBuffer *q, wl_resource *resource);

    ShmClientBuffer *q;
    ShmPool *pool;
    int32_t width;
    int32_t height;
    int32_t offset;
    int32_t stride;
    QImage::Format format;
    bool hasAlphaChannel;

protected:
    void buffer_destroy(Resource *resource) override;
};

ShmMapping::ShmMapping(int fd, int32_t size)
    : data(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))
    , size(size)
{
}

ShmMapping::~ShmMapping()
{
    if (isValid()) {
        munmap(data, size);
    }
}

bool ShmMapping::isValid() const
{
    return data != MAP_FAILED;
}

ShmPool::ShmPool(ShmClientBufferIntegration *integration, wl_client *client, int id, uint32_t version, FileDescriptor &&fd, std::shared_ptr<ShmMapping> mapping)
    : QtWaylandServer::wl_shm_pool(client, id, version)
    , integration(integration)
    , mapping(mapping)
    , fd(std::move(fd))
{
}

ShmPool::~ShmPool()
{
}

void ShmPool::ref()
{
    ++refCount;
}

void ShmPool::unref()
{
    --refCount;
    if (refCount == 0) {
        delete this;
    }
}

void ShmPool::shm_pool_destroy_resource(Resource *resource)
{
    unref();
}

void ShmPool::shm_pool_create_buffer(Resource *resource, uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
{
    if (!integration->formats().contains(format)) {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_FORMAT,
                               "invalid format 0x%x",
                               format);
        return;
    }

    if (offset < 0 || width <= 0 || height <= 0 || stride < width) {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_STRIDE,
                               "invalid width, height or stride (%dx%d, %u)",
                               width, height, stride);
        return;
    }

    wl_resource *bufferResource = wl_resource_create(resource->client(), &wl_buffer_interface, 1, id);

    auto buffer = new ShmClientBuffer(this, offset, width, height, stride, format, bufferResource);
    DisplayPrivate *displayPrivate = DisplayPrivate::get(integration->display());
    displayPrivate->registerClientBuffer(buffer);
}

void ShmPool::shm_pool_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShmPool::shm_pool_resize(Resource *resource, int32_t size)
{
    if (size < mapping->size) {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_FD,
                               "shrinking pool invalid");
        return;
    }

    auto remapping = std::make_shared<ShmMapping>(fd.get(), size);
    if (remapping->isValid()) {
        mapping = remapping;
    } else {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_FD,
                               "failed to map shm pool with the new size");
    }
}

ShmClientBufferPrivate::ShmClientBufferPrivate(ShmClientBuffer *q, wl_resource *resource)
    : QtWaylandServer::wl_buffer(resource)
    , q(q)
{
}

void ShmClientBufferPrivate::buffer_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static bool alphaChannelFromFormat(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ABGR16161616:
    case WL_SHM_FORMAT_ABGR2101010:
    case WL_SHM_FORMAT_ARGB2101010:
    case WL_SHM_FORMAT_ARGB8888:
        return true;
    case WL_SHM_FORMAT_XBGR16161616:
    case WL_SHM_FORMAT_XBGR2101010:
    case WL_SHM_FORMAT_XRGB2101010:
    case WL_SHM_FORMAT_XRGB8888:
    default:
        return false;
    }
}

static QImage::Format imageFormatForShmFormat(uint32_t format)
{
    switch (format) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    case WL_SHM_FORMAT_ABGR16161616:
        return QImage::Format_RGBA64_Premultiplied;
    case WL_SHM_FORMAT_XBGR16161616:
        return QImage::Format_RGBX64;
    case WL_SHM_FORMAT_ARGB2101010:
        return QImage::Format_A2RGB30_Premultiplied;
    case WL_SHM_FORMAT_XRGB2101010:
        return QImage::Format_RGB30;
    case WL_SHM_FORMAT_ABGR2101010:
        return QImage::Format_A2BGR30_Premultiplied;
    case WL_SHM_FORMAT_XBGR2101010:
        return QImage::Format_BGR30;
#endif
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

ShmClientBuffer::ShmClientBuffer(ShmPool *pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format, wl_resource *resource)
    : ClientBuffer(resource, *new ShmClientBufferPrivate(this, resource))
{
    Q_D(ShmClientBuffer);
    d->pool = pool;
    d->pool->ref();

    d->width = width;
    d->height = height;
    d->offset = offset;
    d->stride = stride;
    d->format = imageFormatForShmFormat(format);
    d->hasAlphaChannel = alphaChannelFromFormat(format);
}

ShmClientBuffer::~ShmClientBuffer()
{
    Q_D(ShmClientBuffer);
    d->pool->unref();
}

QSize ShmClientBuffer::size() const
{
    Q_D(const ShmClientBuffer);
    return QSize(d->width, d->height);
}

bool ShmClientBuffer::hasAlphaChannel() const
{
    Q_D(const ShmClientBuffer);
    return d->hasAlphaChannel;
}

ClientBuffer::Origin ShmClientBuffer::origin() const
{
    return Origin::TopLeft;
}

static void sigbusHandler(int signum, siginfo_t *info, void *context)
{
    auto reraise = [&]() {
        if (prevSigbusAction.sa_flags & SA_SIGINFO) {
            prevSigbusAction.sa_sigaction(signum, info, context);
        } else {
            prevSigbusAction.sa_handler(signum);
        }
    };

    ShmSigbusData *sigbusData = static_cast<ShmSigbusData *>(pthread_getspecific(sigbusDataKey));
    if (!sigbusData || !sigbusData->mapping) {
        reraise();
        return;
    }

    if (mmap(sigbusData->mapping->data, sigbusData->mapping->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
        reraise();
        return;
    }
}

QImage ShmClientBuffer::data() const
{
    Q_D(const ShmClientBuffer);

    pthread_once(&sigbusOnce, []() {
        struct sigaction action {
            .sa_sigaction = sigbusHandler,
            .sa_flags = SA_SIGINFO | SA_NODEFER,
        };

        sigemptyset(&action.sa_mask);
        sigaction(SIGBUS, &action, &prevSigbusAction);
        pthread_key_create(&sigbusDataKey, [](void *data) {
            ShmSigbusData *sigbusData = static_cast<ShmSigbusData *>(data);
            delete sigbusData;
        });
    });

    ShmSigbusData *sigbusData = static_cast<ShmSigbusData *>(pthread_getspecific(sigbusDataKey));
    if (!sigbusData) {
        sigbusData = new ShmSigbusData;
        pthread_setspecific(sigbusDataKey, sigbusData);
    }

    const auto &mapping = d->pool->mapping;
    sigbusData->mapping = mapping;
    ++sigbusData->accessCount;

    return QImage(reinterpret_cast<uchar *>(mapping->data) + d->offset, d->width, d->height, d->stride, d->format, [](void *) {
        ShmSigbusData *sigbusData = static_cast<ShmSigbusData *>(pthread_getspecific(sigbusDataKey));
        --sigbusData->accessCount;
        if (sigbusData->accessCount == 0) {
            sigbusData->mapping.reset();
        }
    });
}

ShmClientBufferIntegrationPrivate::ShmClientBufferIntegrationPrivate(Display *display, ShmClientBufferIntegration *q)
    : QtWaylandServer::wl_shm(*display, s_version)
    , q(q)
{
}

void ShmClientBufferIntegrationPrivate::shm_bind_resource(Resource *resource)
{
    for (const uint32_t &format : std::as_const(formats)) {
        send_format(resource->handle, format);
    }
}

void ShmClientBufferIntegrationPrivate::shm_create_pool(Resource *resource, uint32_t id, int32_t fd, int32_t size)
{
    FileDescriptor fileDescriptor{fd};

    if (size <= 0) {
        wl_resource_post_error(resource->handle,
                               error_invalid_stride,
                               "invalid size (%d)", size);
        return;
    }

    auto mapping = std::make_shared<ShmMapping>(fd, size);
    if (!mapping->isValid()) {
        wl_resource_post_error(resource->handle,
                               error_invalid_fd,
                               "failed to map shm pool");
        return;
    }

    new ShmPool(q, resource->client(), id, resource->version(), std::move(fileDescriptor), mapping);
}

ShmClientBufferIntegration::ShmClientBufferIntegration(Display *display)
    : ClientBufferIntegration(display)
    , d(new ShmClientBufferIntegrationPrivate(display, this))
{
    d->formats.append(WL_SHM_FORMAT_ARGB8888);
    d->formats.append(WL_SHM_FORMAT_XRGB8888);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    d->formats.append(WL_SHM_FORMAT_ARGB2101010);
    d->formats.append(WL_SHM_FORMAT_XRGB2101010);
    d->formats.append(WL_SHM_FORMAT_ABGR2101010);
    d->formats.append(WL_SHM_FORMAT_XBGR2101010);
    d->formats.append(WL_SHM_FORMAT_ABGR16161616);
    d->formats.append(WL_SHM_FORMAT_XBGR16161616);
#endif
}

ShmClientBufferIntegration::~ShmClientBufferIntegration()
{
}

QVector<uint32_t> ShmClientBufferIntegration::formats() const
{
    return d->formats;
}

} // namespace KWaylandServer
