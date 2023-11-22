/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QSocketNotifier>
#include <memory>
#include <stdint.h>
#include <unordered_set>

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

private:
    const std::shared_ptr<SyncTimeline> m_timeline;
    const uint64_t m_timelinePoint;
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

private:
    const int32_t m_drmFd;
    const uint32_t m_handle;
};

class SyncReleasePointHolder : public QObject
{
    Q_OBJECT
public:
    /**
     * @param requirement the filedescriptor that needs to be readable before the release points may be signalled. Once that's happened, this object deletes itself!'
     */
    explicit SyncReleasePointHolder(FileDescriptor &&requirement, std::unordered_set<std::shared_ptr<SyncReleasePoint>> &&releasePoints);

private:
    void signaled();

    const FileDescriptor m_fence;
    QSocketNotifier m_notifier;
    const std::unordered_set<std::shared_ptr<SyncReleasePoint>> m_releasePoints;
};
}
