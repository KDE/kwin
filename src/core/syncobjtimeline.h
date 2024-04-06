/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <memory>
#include <stdint.h>

namespace KWin
{

class SyncTimeline;

/**
 * A helper to signal the release point when it goes out of scope
 */
class KWIN_EXPORT SyncReleasePoint
{
public:
    explicit SyncReleasePoint(const std::shared_ptr<SyncTimeline> &timeline, uint64_t timelinePoint);
    ~SyncReleasePoint();

    SyncTimeline *timeline() const;
    uint64_t timelinePoint() const;

    /**
     * Adds the fence of a graphics job that this release point should wait for
     * before the timeline point is signaled
     */
    void addReleaseFence(const FileDescriptor &fd);

private:
    const std::shared_ptr<SyncTimeline> m_timeline;
    const uint64_t m_timelinePoint;
    FileDescriptor m_releaseFence;
};

class KWIN_EXPORT SyncTimeline
{
public:
    explicit SyncTimeline(int drmFd, uint32_t handle);
    ~SyncTimeline();

    /**
     * @returns an event fd that gets signalled when the timeline point gets signalled
     */
    FileDescriptor eventFd(uint64_t timelinePoint) const;

    void signal(uint64_t timelinePoint);
    void moveInto(uint64_t timelinePoint, const FileDescriptor &fd);

private:
    const int32_t m_drmFd;
    const uint32_t m_handle;
};
}
