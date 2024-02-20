/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_commit_thread.h"
#include "drm_commit.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "utils/realtime.h"

using namespace std::chrono_literals;

namespace KWin
{

/**
 * This should always be longer than any real pageflip can take, even with PSR and modesets
 */
static constexpr auto s_pageflipTimeout = 5s;

// amdgpu doesn't handle this correctly, so it's off by default
// https://gitlab.freedesktop.org/drm/amd/-/issues/2186
static const bool s_delayVrrCursorUpdates = qEnvironmentVariableIntValue("KWIN_DRM_DELAY_VRR_CURSOR_UPDATES") == 1;

DrmCommitThread::DrmCommitThread(DrmGpu *gpu, const QString &name)
{
    if (!gpu->atomicModeSetting()) {
        return;
    }
    m_thread.reset(QThread::create([this]() {
        const auto thread = QThread::currentThread();
        gainRealTime();
        while (true) {
            if (thread->isInterruptionRequested()) {
                return;
            }
            std::unique_lock lock(m_mutex);
            bool timeout = false;
            if (m_committed) {
                timeout = m_commitPending.wait_for(lock, s_pageflipTimeout) == std::cv_status::timeout;
            } else if (m_commits.empty()) {
                m_commitPending.wait(lock);
            }
            if (m_committed) {
                // the commit would fail with EBUSY, wait until the pageflip is done
                if (timeout) {
                    qCCritical(KWIN_DRM, "Pageflip timed out! This is a kernel bug");
                    std::unique_ptr<DrmAtomicCommit> committed(static_cast<DrmAtomicCommit *>(m_committed.release()));
                    const bool cursorOnly = committed->isCursorOnly();
                    m_droppedCommits.push_back(std::move(committed));
                    if (!cursorOnly) {
                        QMetaObject::invokeMethod(this, &DrmCommitThread::commitFailed, Qt::ConnectionType::QueuedConnection);
                    }
                }
                continue;
            }
            if (m_commits.empty()) {
                continue;
            }
            const auto now = std::chrono::steady_clock::now();
            if (m_targetPageflipTime > now + m_safetyMargin) {
                lock.unlock();
                std::this_thread::sleep_until(m_targetPageflipTime - m_safetyMargin);
                lock.lock();
            }
            optimizeCommits();
            if (!m_commits.front()->areBuffersReadable()) {
                // no commit is ready yet, reschedule
                if (m_vrr) {
                    m_targetPageflipTime += 50us;
                } else {
                    m_targetPageflipTime += m_minVblankInterval;
                }
                continue;
            }
            if (m_commits.front()->isCursorOnly() && m_vrr && s_delayVrrCursorUpdates) {
                // wait for a primary plane commit to be in, while still enforcing
                // a minimum cursor refresh rate of 30Hz
                const auto cursorTarget = m_lastPageflip + std::chrono::duration_cast<std::chrono::nanoseconds>(1s) / 30;
                const bool cursorOnly = std::all_of(m_commits.begin(), m_commits.end(), [](const auto &commit) {
                    return commit->isCursorOnly();
                });
                if (cursorOnly) {
                    // no primary plane commit, just wait until a new one gets added or the cursorTarget time is reached
                    if (m_commitPending.wait_until(lock, cursorTarget) == std::cv_status::no_timeout) {
                        continue;
                    }
                } else {
                    bool timeout = true;
                    while (std::chrono::steady_clock::now() < cursorTarget && timeout && m_commits.front()->isCursorOnly()) {
                        timeout = m_commitPending.wait_for(lock, 50us) == std::cv_status::timeout;
                        optimizeCommits();
                    }
                    if (!timeout) {
                        // some new commit was added, process that
                        continue;
                    }
                }
            }
            submit();
        }
    }));
    m_thread->setObjectName(name);
    m_thread->start();
}

void DrmCommitThread::submit()
{
    auto &commit = m_commits.front();
    const auto vrr = commit->isVrr();
    const bool success = commit->commit();
    if (success) {
        m_vrr = vrr.value_or(m_vrr);
        m_committed = std::move(commit);
        m_commits.erase(m_commits.begin());
    } else {
        if (m_commits.size() > 1) {
            // the failure may have been because of the reordering of commits
            // -> collapse all commits into one and try again with an already tested state
            while (m_commits.size() > 1) {
                auto toMerge = std::move(m_commits[1]);
                m_commits.erase(m_commits.begin() + 1);
                m_commits.front()->merge(toMerge.get());
                m_droppedCommits.push_back(std::move(toMerge));
            }
            if (commit->test()) {
                // presentation didn't fail after all, try again
                submit();
                return;
            }
        }
        const bool cursorOnly = std::all_of(m_commits.begin(), m_commits.end(), [](const auto &commit) {
            return commit->isCursorOnly();
        });
        for (auto &commit : m_commits) {
            m_droppedCommits.push_back(std::move(commit));
        }
        m_commits.clear();
        qCWarning(KWIN_DRM) << "atomic commit failed:" << strerror(errno);
        if (!cursorOnly) {
            QMetaObject::invokeMethod(this, &DrmCommitThread::commitFailed, Qt::ConnectionType::QueuedConnection);
        }
    }
    QMetaObject::invokeMethod(this, &DrmCommitThread::clearDroppedCommits, Qt::ConnectionType::QueuedConnection);
}

void DrmCommitThread::optimizeCommits()
{
    if (m_commits.size() <= 1) {
        return;
    }
    // merge commits in the front that are already ready (regardless of which planes they modify)
    if (m_commits.front()->areBuffersReadable()) {
        auto it = m_commits.begin() + 1;
        while (it != m_commits.end() && (*it)->areBuffersReadable()) {
            m_commits.front()->merge(it->get());
            m_droppedCommits.push_back(std::move(*it));
            it = m_commits.erase(it);
        }
    }
    // merge commits that are ready and modify the same drm planes
    for (auto it = m_commits.begin(); it != m_commits.end();) {
        DrmAtomicCommit *const commit = it->get();
        it++;
        while (it != m_commits.end() && commit->modifiedPlanes() == (*it)->modifiedPlanes() && (*it)->areBuffersReadable()) {
            commit->merge(it->get());
            m_droppedCommits.push_back(std::move(*it));
            it = m_commits.erase(it);
        }
    }
    if (m_commits.size() == 1) {
        // already done
        return;
    }
    std::unique_ptr<DrmAtomicCommit> front;
    if (m_commits.front()->areBuffersReadable()) {
        front = std::move(m_commits.front());
        m_commits.erase(m_commits.begin());
    }
    // try to move commits that are ready to the front
    for (auto it = m_commits.begin() + 1; it != m_commits.end();) {
        auto &commit = *it;
        // commits that target the same plane(s) need to stay in the same order
        const auto &planes = commit->modifiedPlanes();
        const bool skipping = std::any_of(m_commits.begin(), it, [&planes](const auto &other) {
            return std::any_of(planes.begin(), planes.end(), [&other](DrmPlane *plane) {
                return other->modifiedPlanes().contains(plane);
            });
        });
        if (skipping || !commit->areBuffersReadable()) {
            it++;
            continue;
        }
        // find out if the modified commit order will actually work
        std::unique_ptr<DrmAtomicCommit> duplicate;
        if (front) {
            duplicate = std::make_unique<DrmAtomicCommit>(*front);
            duplicate->merge(commit.get());
            if (!duplicate->test()) {
                m_droppedCommits.push_back(std::move(duplicate));
                it++;
                continue;
            }
        } else {
            if (!commit->test()) {
                it++;
                continue;
            }
            duplicate = std::make_unique<DrmAtomicCommit>(*commit);
        }
        bool success = true;
        for (const auto &otherCommit : m_commits) {
            if (otherCommit != commit) {
                duplicate->merge(otherCommit.get());
                if (!duplicate->test()) {
                    success = false;
                    break;
                }
            }
        }
        m_droppedCommits.push_back(std::move(duplicate));
        if (success) {
            if (front) {
                front->merge(commit.get());
                m_droppedCommits.push_back(std::move(commit));
            } else {
                front = std::move(commit);
            }
            it = m_commits.erase(it);
        } else {
            it++;
        }
    }
    if (front) {
        m_commits.insert(m_commits.begin(), std::move(front));
    }
}

DrmCommitThread::~DrmCommitThread()
{
    if (m_thread) {
        m_thread->requestInterruption();
        m_commitPending.notify_all();
        m_thread->wait();
    }
}

void DrmCommitThread::addCommit(std::unique_ptr<DrmAtomicCommit> &&commit)
{
    std::unique_lock lock(m_mutex);
    m_commits.push_back(std::move(commit));
    const auto now = std::chrono::steady_clock::now();
    if (m_vrr && now >= m_lastPageflip + m_minVblankInterval) {
        m_targetPageflipTime = now;
    } else {
        m_targetPageflipTime = estimateNextVblank(now);
    }
    m_commits.back()->setDeadline(m_targetPageflipTime - m_safetyMargin);
    m_commitPending.notify_all();
}

void DrmCommitThread::setPendingCommit(std::unique_ptr<DrmLegacyCommit> &&commit)
{
    m_committed = std::move(commit);
}

void DrmCommitThread::clearDroppedCommits()
{
    std::unique_lock lock(m_mutex);
    m_droppedCommits.clear();
}

void DrmCommitThread::setModeInfo(uint32_t maximum, std::chrono::nanoseconds vblankTime)
{
    std::unique_lock lock(m_mutex);
    m_minVblankInterval = std::chrono::nanoseconds(1'000'000'000'000ull / maximum);
    // the kernel rejects commits that happen during vblank
    // the 1.5ms on top of that was chosen experimentally, for the time it takes to commit + scheduling inaccuracies
    m_safetyMargin = vblankTime + 1500us;
}

void DrmCommitThread::pageFlipped(std::chrono::nanoseconds timestamp)
{
    std::unique_lock lock(m_mutex);
    m_lastPageflip = TimePoint(timestamp);
    m_committed.reset();
    if (!m_commits.empty()) {
        m_targetPageflipTime = estimateNextVblank(std::chrono::steady_clock::now());
        m_commitPending.notify_all();
    }
}

bool DrmCommitThread::pageflipsPending()
{
    std::unique_lock lock(m_mutex);
    return !m_commits.empty() || m_committed;
}

TimePoint DrmCommitThread::estimateNextVblank(TimePoint now) const
{
    // the pageflip timestamp may be in the future
    const uint64_t pageflipsSince = now >= m_lastPageflip ? (now - m_lastPageflip) / m_minVblankInterval : 0;
    return m_lastPageflip + m_minVblankInterval * (pageflipsSince + 1);
}

std::chrono::nanoseconds DrmCommitThread::safetyMargin() const
{
    return m_safetyMargin;
}
}
