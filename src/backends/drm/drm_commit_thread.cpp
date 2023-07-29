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

using namespace std::chrono_literals;

namespace KWin
{

// This value was chosen experimentally and should be adjusted if needed
// committing takes about 800µs, the rest is accounting for sleep not being accurate enough
static constexpr auto s_safetyMargin = 1800us;

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
                const auto now = std::chrono::steady_clock::now();
                if (m_targetPageflipTime > now + s_safetyMargin) {
                    lock.unlock();
                    std::this_thread::sleep_until(m_targetPageflipTime - s_safetyMargin);
                    lock.lock();
                }
                // the other thread may replace the commit, but not erase it
                Q_ASSERT(m_commit);
                if (!m_commit->areBuffersReadable()) {
                    // reschedule, this commit would not hit the pageflip deadline anyways
                    if (m_vrr) {
                        m_targetPageflipTime += 50us;
                    } else {
                        m_targetPageflipTime += m_minVblankInterval;
                    }
                    continue;
                }
                const bool vrr = m_commit->isVrr();
                if (m_commit->commit()) {
                    m_vrr = vrr;
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

void DrmCommitThread::setCommit(std::unique_ptr<DrmAtomicCommit> &&commit)
{
    std::unique_lock lock(m_mutex);
    m_commit = std::move(commit);
    const auto now = std::chrono::steady_clock::now();
    if (m_vrr && now >= m_lastPageflip + m_minVblankInterval) {
        m_targetPageflipTime = now;
    } else {
        m_targetPageflipTime = estimateNextVblank(now);
    }
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

void DrmCommitThread::setRefreshRate(uint32_t maximum)
{
    std::unique_lock lock(m_mutex);
    m_minVblankInterval = std::chrono::nanoseconds(1'000'000'000'000ull / maximum);
}

void DrmCommitThread::pageFlipped(std::chrono::nanoseconds timestamp)
{
    std::unique_lock lock(m_mutex);
    m_lastPageflip = TimePoint(timestamp);
}

TimePoint DrmCommitThread::estimateNextVblank(TimePoint now) const
{
    const uint64_t pageflipsSince = (now - m_lastPageflip) / m_minVblankInterval;
    return m_lastPageflip + m_minVblankInterval * (pageflipsSince + 1);
}
}
