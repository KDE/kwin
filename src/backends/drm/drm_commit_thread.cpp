/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_commit_thread.h"
#include "drm_commit.h"
#include "drm_gpu.h"
#include "logging_p.h"
#include "utils/realtime.h"

namespace KWin
{

// This value was chosen experimentally and should be adjusted if needed
static constexpr std::chrono::microseconds s_safetyMargin(1000);

DrmCommitThread::DrmCommitThread()
{
    m_thread.reset(QThread::create([this]() {
        gainRealTime();
        while (true) {
            if (QThread::currentThread()->isInterruptionRequested()) {
                return;
            }
            std::unique_lock lock(m_mutex);
            if (!m_commit) {
                m_commitPending.wait(lock);
            }
            if (m_commit) {
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                if (m_targetPageflipTime > now + s_safetyMargin) {
                    lock.unlock();
                    std::this_thread::sleep_for(m_targetPageflipTime - s_safetyMargin - now);
                    lock.lock();
                }
                // the other thread may replace the commit, but not erase it
                Q_ASSERT(m_commit);
                if (m_commit->commit()) {
                    // the atomic commit takes ownership of the object
                    m_commit.release();
                } else {
                    qCWarning(KWIN_DRM) << "atomic commit failed:" << strerror(errno);
                    m_droppedCommits.push_back(std::move(m_commit));
                    QMetaObject::invokeMethod(this, &DrmCommitThread::commitFailed, Qt::ConnectionType::QueuedConnection);
                    QMetaObject::invokeMethod(this, &DrmCommitThread::clearDroppedCommits, Qt::ConnectionType::QueuedConnection);
                }
            }
        }
    }));
    m_thread->start();
}

DrmCommitThread::~DrmCommitThread()
{
    m_thread->requestInterruption();
    m_commitPending.notify_all();
    m_thread->wait();
}

void DrmCommitThread::setCommit(std::unique_ptr<DrmAtomicCommit> &&commit, std::chrono::nanoseconds targetPageflipTime)
{
    std::unique_lock lock(m_mutex);
    m_commit = std::move(commit);
    m_targetPageflipTime = targetPageflipTime;
    m_commitPending.notify_all();
}

bool DrmCommitThread::replaceCommit(std::unique_ptr<DrmAtomicCommit> &&commit)
{
    std::unique_lock lock(m_mutex);
    if (m_commit) {
        m_commit = std::move(commit);
        m_commitPending.notify_all();
        return true;
    } else {
        return false;
    }
}

void DrmCommitThread::clearDroppedCommits()
{
    std::unique_lock lock(m_mutex);
    m_droppedCommits.clear();
}
}
