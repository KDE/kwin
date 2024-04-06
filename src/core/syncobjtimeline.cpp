/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "syncobjtimeline.h"

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

SyncReleasePoint::SyncReleasePoint(const std::shared_ptr<SyncTimeline> &timeline, uint64_t timelinePoint)
    : m_timeline(timeline)
    , m_timelinePoint(timelinePoint)
{
}

SyncReleasePoint::~SyncReleasePoint()
{
    if (m_releaseFence.isValid()) {
        m_timeline->moveInto(m_timelinePoint, m_releaseFence);
    } else {
        m_timeline->signal(m_timelinePoint);
    }
}

static FileDescriptor mergeSyncFds(const FileDescriptor &fd1, const FileDescriptor &fd2)
{
    struct sync_merge_data data
    {
        .name = "merged release fence",
        .fd2 = fd2.get(),
        .fence = -1,
    };
    int err = -1;
    do {
        err = ioctl(fd1.get(), SYNC_IOC_MERGE, &data);
    } while (err == -1 && (errno == EINTR || errno == EAGAIN));
    if (err < 0) {
        return FileDescriptor{};
    } else {
        return FileDescriptor(data.fence);
    }
}

void SyncReleasePoint::addReleaseFence(const FileDescriptor &fd)
{
    if (m_releaseFence.isValid()) {
        m_releaseFence = mergeSyncFds(m_releaseFence, fd);
    } else {
        m_releaseFence = fd.duplicate();
    }
}

SyncTimeline *SyncReleasePoint::timeline() const
{
    return m_timeline.get();
}

uint64_t SyncReleasePoint::timelinePoint() const
{
    return m_timelinePoint;
}

SyncTimeline::SyncTimeline(int drmFd, uint32_t handle)
    : m_drmFd(drmFd)
    , m_handle(handle)
{
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
    drmSyncobjImportSyncFile(m_drmFd, m_handle, fd.get());
}
}
