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
class DrmAtomicCommit;

using TimePoint = std::chrono::steady_clock::time_point;

class DrmCommitThread : public QObject
{
    Q_OBJECT
public:
    explicit DrmCommitThread(const QString &name);
    ~DrmCommitThread();

    void addCommit(std::unique_ptr<DrmAtomicCommit> &&commit);

    void setRefreshRate(uint32_t maximum);
    void pageFlipped(std::chrono::nanoseconds timestamp);
    bool pageflipsPending();

Q_SIGNALS:
    void commitFailed();

private:
    void clearDroppedCommits();
    TimePoint estimateNextVblank(TimePoint now) const;
    void optimizeCommits();

    std::vector<std::unique_ptr<DrmAtomicCommit>> m_commits;
    std::unique_ptr<QThread> m_thread;
    std::mutex m_mutex;
    std::condition_variable m_commitPending;
    TimePoint m_lastPageflip;
    TimePoint m_targetPageflipTime;
    std::chrono::nanoseconds m_minVblankInterval;
    std::vector<std::unique_ptr<DrmAtomicCommit>> m_droppedCommits;
    bool m_vrr = false;
    bool m_pageflipPending = false;
};

}
