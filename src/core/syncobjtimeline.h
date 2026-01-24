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
 * An abstract helper to signal a synchronization mechanism when it gets out of scope
 */
class KWIN_EXPORT SyncReleasePoint
{
public:
    virtual ~SyncReleasePoint() = default;

    virtual void addReleaseFence(const FileDescriptor &fd) = 0;

protected:
    explicit SyncReleasePoint() = default;
};

/**
 * A helper to signal the release point of a syncobj timeline when it goes out of scope
 */
class KWIN_EXPORT SyncObjReleasePoint : public SyncReleasePoint
{
public:
    explicit SyncObjReleasePoint(const std::shared_ptr<SyncTimeline> &timeline, uint64_t timelinePoint);
    ~SyncObjReleasePoint() override;

    SyncTimeline *timeline() const;
    uint64_t timelinePoint() const;

    /**
     * Adds the fence of a graphics job that this release point should wait for
     * before the timeline point is signaled
     */
    void addReleaseFence(const FileDescriptor &fd) override;

private:
    const std::shared_ptr<SyncTimeline> m_timeline;
    const uint64_t m_timelinePoint;
    FileDescriptor m_releaseFence;
};

class KWIN_EXPORT SyncTimeline
{
public:
    explicit SyncTimeline(int drmFd, uint32_t handle);
    explicit SyncTimeline(int drmFd);
    ~SyncTimeline();

    /**
     * @returns an event fd that gets signalled when the timeline point gets signalled
     */
    FileDescriptor eventFd(uint64_t timelinePoint) const;

    const FileDescriptor &fileDescriptor();
    void signal(uint64_t timelinePoint);
    void moveInto(uint64_t timelinePoint, const FileDescriptor &fd);
    FileDescriptor exportSyncFile(uint64_t timelinePoint);
    bool isMaterialized(uint64_t timelinePoint);

private:
    const int32_t m_drmFd;
    uint32_t m_handle = 0;
    FileDescriptor m_fileDescriptor;
};

}
