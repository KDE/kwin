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

class DrmCommitThread : public QObject
{
    Q_OBJECT
public:
    explicit DrmCommitThread();
    ~DrmCommitThread();

    void setCommit(std::unique_ptr<DrmAtomicCommit> &&commit, std::chrono::nanoseconds targetPageflipTime);
    bool replaceCommit(std::unique_ptr<DrmAtomicCommit> &&commit);

Q_SIGNALS:
    void commitFailed();

private:
    void clearDroppedCommits();

    std::unique_ptr<DrmAtomicCommit> m_commit;
    std::unique_ptr<QThread> m_thread;
    std::mutex m_mutex;
    std::condition_variable m_commitPending;
    std::chrono::nanoseconds m_targetPageflipTime;
    std::vector<std::unique_ptr<DrmAtomicCommit>> m_droppedCommits;
};

}
