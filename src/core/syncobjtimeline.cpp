/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "syncobjtimeline.h"

#include <sys/eventfd.h>
#include <xf86drm.h>

namespace KWin
{

SyncReleasePoint::SyncReleasePoint(const std::shared_ptr<SyncTimeline> &timeline, uint64_t timelinePoint)
    : m_timeline(timeline)
    , m_timelinePoint(timelinePoint)
{
}

SyncReleasePoint::~SyncReleasePoint()
{
    m_timeline->signal(m_timelinePoint);
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

SyncReleasePointHolder::SyncReleasePointHolder(FileDescriptor &&requirement, std::unordered_set<std::shared_ptr<SyncReleasePoint>> &&releasePoints)
    : m_fence(std::move(requirement))
    , m_notifier(m_fence.get(), QSocketNotifier::Type::Read)
    , m_releasePoints(std::move(releasePoints))
{
    connect(&m_notifier, &QSocketNotifier::activated, this, &SyncReleasePointHolder::signaled);
    m_notifier.setEnabled(true);
}

void SyncReleasePointHolder::signaled()
{
    delete this;
}
}
