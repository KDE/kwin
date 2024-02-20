/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QThread>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace KWin
{

class DrmGpu;
class DrmCommit;
class DrmAtomicCommit;
class DrmLegacyCommit;

using TimePoint = std::chrono::steady_clock::time_point;

class DrmCommitThread : public QObject
{
    Q_OBJECT
public:
    explicit DrmCommitThread(DrmGpu *gpu, const QString &name);
    ~DrmCommitThread();

    void addCommit(std::unique_ptr<DrmAtomicCommit> &&commit);
    void setPendingCommit(std::unique_ptr<DrmLegacyCommit> &&commit);

    void setModeInfo(uint32_t maximum, std::chrono::nanoseconds vblankTime);
    void pageFlipped(std::chrono::nanoseconds timestamp);
    bool pageflipsPending();
    /**
     * @return how long before the desired presentation timestamp the commit has to be added
     *         in order to get presented at that timestamp
     */
    std::chrono::nanoseconds safetyMargin() const;

Q_SIGNALS:
    void commitFailed();

private:
    void clearDroppedCommits();
    TimePoint estimateNextVblank(TimePoint now) const;
    void optimizeCommits();
    void submit();

    std::unique_ptr<DrmCommit> m_committed;
    std::vector<std::unique_ptr<DrmAtomicCommit>> m_commits;
    std::unique_ptr<QThread> m_thread;
    std::mutex m_mutex;
    std::condition_variable m_commitPending;
    TimePoint m_lastPageflip;
    TimePoint m_targetPageflipTime;
    std::chrono::nanoseconds m_minVblankInterval;
    std::vector<std::unique_ptr<DrmAtomicCommit>> m_droppedCommits;
    bool m_vrr = false;
    std::chrono::nanoseconds m_safetyMargin{0};
};

}
