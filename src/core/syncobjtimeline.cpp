/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "syncobjtimeline.h"

#include <cerrno>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

#if defined(Q_OS_LINUX)
#include <linux/sync_file.h>
#else
struct sync_merge_data
{
    char name[32];
    __s32 fd2;
    __s32 fence;
    __u32 flags;
    __u32 pad;
};
#define SYNC_IOC_MAGIC '>'
#define SYNC_IOC_MERGE _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#endif

namespace KWin
{

FileDescriptor SyncReleasePoint::mergeSyncFds(const FileDescriptor &one, const FileDescriptor &two)
{
    struct sync_merge_data data{
        .name = "merged release fence",
        .fd2 = two.get(),
        .fence = -1,
    };
    int err = -1;
    do {
        err = ioctl(one.get(), SYNC_IOC_MERGE, &data);
    } while (err == -1 && (errno == EINTR || errno == EAGAIN));
    if (err < 0) {
        return FileDescriptor{};
    } else {
        return FileDescriptor(data.fence);
    }
}

SyncReferencer::SyncReferencer(GraphicsBuffer *buffer, FileDescriptor &&syncFd)
    : m_buffer(buffer)
    , m_sync(std::move(syncFd))
    , m_notifier(m_sync.get(), QSocketNotifier::Read)
{
    QObject::connect(&m_notifier, &QSocketNotifier::activated, &m_notifier, [this]() {
        delete this;
    });
    m_notifier.setEnabled(true);
    if (m_sync.isReadable()) {
        delete this;
    }
}

SyncObjReleasePoint::SyncObjReleasePoint(const std::shared_ptr<SyncTimeline> &timeline, uint64_t timelinePoint)
    : m_timeline(timeline)
    , m_timelinePoint(timelinePoint)
{
}

GraphicsBufferReleasePoint::GraphicsBufferReleasePoint()
{
}

GraphicsBufferReleasePoint::~GraphicsBufferReleasePoint()
{
    if (m_bufferRef && m_releaseFd.isValid()) {
        // the buffer needs to stay alive until the
        // release point is signaled
        new SyncReferencer(m_bufferRef.buffer(), std::move(m_releaseFd));
    }
}

void GraphicsBufferReleasePoint::setBuffer(const std::shared_ptr<GraphicsBuffer> &buffer)
{
    m_bufferRef = buffer;
}

void GraphicsBufferReleasePoint::addReleaseFence(const FileDescriptor &fd)
{
    if (m_releaseFd.isValid()) {
        m_releaseFd = SyncReleasePoint::mergeSyncFds(fd, m_releaseFd);
    } else {
        m_releaseFd = fd.duplicate();
    }
}

const FileDescriptor &GraphicsBufferReleasePoint::releaseFd() const
{
    return m_releaseFd;
}

SyncObjReleasePoint::~SyncObjReleasePoint()
{
    if (m_releaseFence.isValid()) {
        m_timeline->moveInto(m_timelinePoint, m_releaseFence);
    } else {
        m_timeline->signal(m_timelinePoint);
    }
}

void SyncObjReleasePoint::addReleaseFence(const FileDescriptor &fd)
{
    if (m_releaseFence.isValid()) {
        m_releaseFence = SyncReleasePoint::mergeSyncFds(m_releaseFence, fd);
    } else {
        m_releaseFence = fd.duplicate();
    }
}

SyncTimeline *SyncObjReleasePoint::timeline() const
{
    return m_timeline.get();
}

uint64_t SyncObjReleasePoint::timelinePoint() const
{
    return m_timelinePoint;
}

SyncTimeline::SyncTimeline(int drmFd, uint32_t handle)
    : m_drmFd(drmFd)
    , m_handle(handle)
{
}

SyncTimeline::SyncTimeline(int drmFd)
    : m_drmFd(drmFd)
{
    drmSyncobjCreate(m_drmFd, 0, &m_handle);
}

const FileDescriptor &SyncTimeline::fileDescriptor()
{
    if (!m_fileDescriptor.isValid()) {
        int fd = -1;
        drmSyncobjHandleToFD(m_drmFd, m_handle, &fd);
        m_fileDescriptor = FileDescriptor(fd);
    }

    return m_fileDescriptor;
}

SyncTimeline::~SyncTimeline()
{
    drmSyncobjDestroy(m_drmFd, m_handle);
}

FileDescriptor SyncTimeline::eventFd(uint64_t timelinePoint) const
{
    FileDescriptor ret{eventfd(0, EFD_CLOEXEC)};
    if (!ret.isValid()) {
        return {};
    }
    if (drmSyncobjEventfd(m_drmFd, m_handle, timelinePoint, ret.get(), 0) != 0) {
        return {};
    }
    return ret;
}

void SyncTimeline::signal(uint64_t timelinePoint)
{
    drmSyncobjTimelineSignal(m_drmFd, &m_handle, &timelinePoint, 1);
}

void SyncTimeline::moveInto(uint64_t timelinePoint, const FileDescriptor &fd)
{
    uint32_t tempHandle = 0;
    drmSyncobjCreate(m_drmFd, 0, &tempHandle);
    drmSyncobjImportSyncFile(m_drmFd, tempHandle, fd.get());
    drmSyncobjTransfer(m_drmFd, m_handle, timelinePoint, tempHandle, 0, 0);
    drmSyncobjDestroy(m_drmFd, tempHandle);
}

FileDescriptor SyncTimeline::exportSyncFile(uint64_t timelinePoint)
{
    uint32_t tempHandle = 0;
    int syncFileFd = -1;
    drmSyncobjCreate(m_drmFd, 0, &tempHandle);
    drmSyncobjTransfer(m_drmFd, tempHandle, 0, m_handle, timelinePoint, 0);
    drmSyncobjExportSyncFile(m_drmFd, tempHandle, &syncFileFd);
    drmSyncobjDestroy(m_drmFd, tempHandle);
    return FileDescriptor(syncFileFd);
}

bool SyncTimeline::isMaterialized(uint64_t timelinePoint)
{
    return (drmSyncobjTimelineWait(m_drmFd,
                                   &m_handle,
                                   &timelinePoint,
                                   1,
                                   0,
                                   DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE,
                                   nullptr)
            == 0);
}

}
