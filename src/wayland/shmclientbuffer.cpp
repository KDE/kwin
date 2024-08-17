/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "config-kwin.h"

#include "utils/drm_format_helper.h"
#include "wayland/display.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/shmclientbuffer_p.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace KWin
{

static constexpr int s_version = 1;

static constexpr uint32_t s_formats[] = {
    WL_SHM_FORMAT_ARGB8888,
    WL_SHM_FORMAT_XRGB8888,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    WL_SHM_FORMAT_ARGB2101010,
    WL_SHM_FORMAT_XRGB2101010,
    WL_SHM_FORMAT_ABGR2101010,
    WL_SHM_FORMAT_XBGR2101010,
    WL_SHM_FORMAT_ABGR16161616,
    WL_SHM_FORMAT_XBGR16161616,
#endif
    WL_SHM_FORMAT_BGR888,
    WL_SHM_FORMAT_RGB888,
};

static std::atomic<ShmAccess *> s_accessedBuffers = nullptr;
static struct sigaction prevSigbusAction;

static uint32_t shmFormatToDrmFormat(uint32_t shmFormat)
{
    switch (shmFormat) {
    case WL_SHM_FORMAT_ARGB8888:
        return DRM_FORMAT_ARGB8888;
    case WL_SHM_FORMAT_XRGB8888:
        return DRM_FORMAT_XRGB8888;
    default:
        return shmFormat; // other wl_shm formats match the drm formats
    }
}

ShmPool::ShmPool(ShmClientBufferIntegration *integration, wl_client *client, int id, uint32_t version, FileDescriptor &&fd, std::shared_ptr<MemoryMap> mapping)
    : QtWaylandServer::wl_shm_pool(client, id, version)
    , integration(integration)
    , mapping(std::move(mapping))
    , fd(std::move(fd))
{
#if HAVE_MEMFD
    const int seals = fcntl(this->fd.get(), F_GET_SEALS);
    if (seals != -1) {
        struct stat statbuf;
        if ((seals & F_SEAL_SHRINK) && fstat(this->fd.get(), &statbuf) >= 0) {
            sigbusImpossible = statbuf.st_size >= this->mapping->size();
        }
    }
#endif
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

void ShmPool::shm_pool_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShmPool::shm_pool_create_buffer(Resource *resource, uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
{
    if (std::find(std::begin(s_formats), std::end(s_formats), format) == std::end(s_formats)) {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_FORMAT,
                               "invalid format 0x%x",
                               format);
        return;
    }

    if (offset < 0 || width <= 0 || height <= 0 || stride < width
        || INT32_MAX / stride < height || offset > mapping->size() - stride * height) {
        wl_resource_post_error(resource->handle,
                               WL_SHM_ERROR_INVALID_STRIDE,
                               "invalid width, height or stride (%dx%d, %u)",
                               width, height, stride);
        return;
    }

    const uint32_t drmFormat = shmFormatToDrmFormat(format);

    if (auto formatInfo = FormatInfo::get(drmFormat)) {
        const uint32_t bytesPerPixel = formatInfo->bitsPerPixel / 8;
        if ((stride % bytesPerPixel) != 0) {
            wl_resource_post_error(resource->handle,
                                   WL_SHM_ERROR_INVALID_STRIDE,
                                   "invalid stride, %d is not a multiple of %d",
                                   stride,
                                   bytesPerPixel);
            return;
        }
    }

    ShmAttributes attributes{
        .fd = fd.duplicate(),
        .stride = stride,
        .offset = offset,
        .size = QSize(width, height),
        .format = drmFormat,
    };

    new ShmClientBuffer(this, std::move(attributes), resource->client(), id);
}

void ShmPool::shm_pool_resize(Resource *resource, int32_t size)
{
    if (size < mapping->size()) {
        wl_resource_post_error(resource->handle, WL_SHM_ERROR_INVALID_FD, "shrinking pool invalid");
        return;
    }

    auto remapping = std::make_shared<MemoryMap>(size, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    if (remapping->isValid()) {
        mapping = std::move(remapping);
    } else {
        wl_resource_post_error(resource->handle, WL_SHM_ERROR_INVALID_FD, "failed to map shm pool with the new size");
    }
}

void ShmClientBuffer::buffer_destroy_resource(wl_resource *resource)
{
    if (ShmClientBuffer *buffer = ShmClientBuffer::get(resource)) {
        buffer->m_resource = nullptr;
        buffer->drop();
    }
}

void ShmClientBuffer::buffer_destroy(wl_client *client, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

const struct wl_buffer_interface ShmClientBuffer::implementation = {
    .destroy = buffer_destroy,
};

ShmClientBuffer::ShmClientBuffer(ShmPool *pool, ShmAttributes attributes, wl_client *client, uint32_t id)
    : m_shmPool(pool)
    , m_shmAttributes(std::move(attributes))
{
    m_shmPool->ref();

    connect(this, &GraphicsBuffer::released, [this]() {
        wl_buffer_send_release(m_resource);
    });

    m_resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
    wl_resource_set_implementation(m_resource, &implementation, this, buffer_destroy_resource);
}

ShmClientBuffer::~ShmClientBuffer()
{
    m_shmPool->unref();
}

QSize ShmClientBuffer::size() const
{
    return m_shmAttributes.size;
}

bool ShmClientBuffer::hasAlphaChannel() const
{
    return alphaChannelFromDrmFormat(m_shmAttributes.format);
}

const ShmAttributes *ShmClientBuffer::shmAttributes() const
{
    return &m_shmAttributes;
}

ShmClientBuffer *ShmClientBuffer::get(wl_resource *resource)
{
    if (wl_resource_instance_of(resource, &wl_buffer_interface, &implementation)) {
        return static_cast<ShmClientBuffer *>(wl_resource_get_user_data(resource));
    }
    return nullptr;
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

    MemoryMap *mapping = nullptr;
    for (auto access = s_accessedBuffers.load(); access; access = access->next) {
        const uchar *addr = static_cast<uchar *>(info->si_addr);
        const uchar *mappingStart = static_cast<uchar *>(access->mapping->data());
        if (addr >= mappingStart && addr < mappingStart + access->mapping->size()) {
            mapping = access->mapping.get();
            break;
        }
    }

    if (!mapping) {
        reraise();
        return;
    }

    // Replace the faulty mapping with a new one that's filled with zeros.
    if (mmap(mapping->data(), mapping->size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
        reraise();
        return;
    }
}

GraphicsBuffer::Map ShmClientBuffer::map(MapFlags flags)
{
    if (!m_shmPool->sigbusImpossible) {
        // A SIGBUS signal may be emitted if the backing file is shrinked and we access now
        // removed pages. Install a signal handler to handle this case. Note that if the
        // backing file has F_SEAL_SHRINK seal, then we don't need to do anything.

        static std::once_flag sigbusOnce;
        std::call_once(sigbusOnce, []() {
            struct sigaction action;
            memset(&action, 0, sizeof(action));
            sigemptyset(&action.sa_mask);
            action.sa_sigaction = sigbusHandler;
            action.sa_flags = SA_SIGINFO | SA_NODEFER;
            sigaction(SIGBUS, &action, &prevSigbusAction);
        });
    }

    if (!m_shmAccess.has_value()) {
        ShmAccess &access = m_shmAccess.emplace(m_shmPool->mapping, 0, s_accessedBuffers.load());
        s_accessedBuffers = &access;
    }

    m_shmAccess->count++;
    return Map{
        .data = reinterpret_cast<uchar *>(m_shmAccess->mapping->data()) + m_shmAttributes.offset,
        .stride = uint32_t(m_shmAttributes.stride),
    };
}

void ShmClientBuffer::unmap()
{
    if (!m_shmAccess.has_value()) {
        return;
    }

    m_shmAccess->count--;
    if (m_shmAccess->count != 0) {
        return;
    }

    if (s_accessedBuffers == &m_shmAccess.value()) {
        s_accessedBuffers = m_shmAccess->next.load();
    } else {
        for (auto access = s_accessedBuffers.load(); access; access = access->next) {
            if (access->next == &m_shmAccess.value()) {
                access->next = m_shmAccess->next.load();
                break;
            }
        }
    }

    m_shmAccess.reset();
}

ShmClientBufferIntegrationPrivate::ShmClientBufferIntegrationPrivate(Display *display, ShmClientBufferIntegration *q)
    : QtWaylandServer::wl_shm(*display, s_version)
    , q(q)
{
}

void ShmClientBufferIntegrationPrivate::shm_bind_resource(Resource *resource)
{
    for (const uint32_t &format : s_formats) {
        send_format(resource->handle, format);
    }
}

void ShmClientBufferIntegrationPrivate::shm_create_pool(Resource *resource, uint32_t id, int32_t fd, int32_t size)
{
    FileDescriptor fileDescriptor{fd};

    if (size <= 0) {
        wl_resource_post_error(resource->handle, error_invalid_stride, "invalid size (%d)", size);
        return;
    }

    auto mapping = std::make_shared<MemoryMap>(size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!mapping->isValid()) {
        wl_resource_post_error(resource->handle, error_invalid_fd, "failed to map shm pool");
        return;
    }

    new ShmPool(q, resource->client(), id, resource->version(), std::move(fileDescriptor), std::move(mapping));
}

ShmClientBufferIntegration::ShmClientBufferIntegration(Display *display)
    : QObject(display)
    , d(new ShmClientBufferIntegrationPrivate(display, this))
{
}

ShmClientBufferIntegration::~ShmClientBufferIntegration()
{
}

} // namespace KWin

#include "moc_shmclientbuffer_p.cpp"

#include "moc_shmclientbuffer.cpp"
