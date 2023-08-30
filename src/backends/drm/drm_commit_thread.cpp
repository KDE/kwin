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

static int64_t micros(auto time)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(time).count();
}

DrmCommitThread::DrmCommitThread()
{
    m_thread.reset(QThread::create([this]() {
        RenderJournal creationToCommit;
        uint64_t droppedCommits = 0;
        auto lastPrint = std::chrono::steady_clock::now();
        gainRealTime();
        while (true) {
            if (QThread::currentThread()->isInterruptionRequested()) {
                return;
            }
            std::unique_lock lock(m_mutex);
            if (m_commits.empty()) {
                m_commitPending.wait(lock);
            }
            if (m_pageflipPending) {
                // the commit would fail with EBUSY, wait until the pageflip is done
                continue;
            }
            if (!m_commits.empty()) {
                const auto now = std::chrono::steady_clock::now();
                if (m_targetPageflipTime > now + s_safetyMargin) {
                    lock.unlock();
                    std::this_thread::sleep_until(m_targetPageflipTime - s_safetyMargin);
                    lock.lock();
                }
                optimizeCommits();
                auto &commit = m_commits.front();
                if (!commit->firstCheck()) {
                    creationToCommit.add(std::chrono::steady_clock::now() - commit->creationTime());
                    commit->setFirstCheck();
                }
                if (!commit->areBuffersReadable()) {
                    droppedCommits++;
                    // no commit is ready yet, reschedule
                    if (m_vrr) {
                        m_targetPageflipTime += 50us;
                    } else {
                        m_targetPageflipTime += m_minVblankInterval;
                    }
                    continue;
                }
                const auto vrr = commit->isVrr();
                m_lastCommitTarget = m_targetPageflipTime;
                const bool success = commit->commit();
                if (success) {
                    m_pageflipPending = true;
                    m_vrr = vrr.value_or(m_vrr);
                    // the atomic commit takes ownership of the object
                    commit.release();
                    m_commits.erase(m_commits.begin());
                } else {
                    for (auto &commit : m_commits) {
                        m_droppedCommits.push_back(std::move(commit));
                    }
                    m_commits.clear();
                    qCWarning(KWIN_DRM) << "atomic commit failed:" << strerror(errno);
                    QMetaObject::invokeMethod(this, &DrmCommitThread::commitFailed, Qt::ConnectionType::QueuedConnection);
                }
                QMetaObject::invokeMethod(this, &DrmCommitThread::clearDroppedCommits, Qt::ConnectionType::QueuedConnection);
                if (std::chrono::steady_clock::now() > lastPrint + 1s) {
                    if (droppedCommits > 0 || m_missedPageflips > 0) {
                        qWarning() << "dropped" << droppedCommits << "and missed" << m_missedPageflips << "commits over the last second";
                        droppedCommits = 0;
                        m_missedPageflips = 0;
                    }
                    qWarning() << "creation to buffercheck time:" << micros(creationToCommit.average()) << micros(creationToCommit.minimum()) << micros(creationToCommit.maximum());
                    lastPrint = std::chrono::steady_clock::now();
                }
            }
        }
    }));
    m_thread->start();
}

void DrmCommitThread::optimizeCommits()
{
    if (m_commits.size() <= 1) {
        return;
    }
    const auto mergeSequentials = [this]() {
        for (auto it = m_commits.begin(); it != m_commits.end();) {
            auto &commit = *it;
            it++;
            if (!commit->areBuffersReadable()) {
                continue;
            }
            while (it != m_commits.end() && (*it)->areBuffersReadable()) {
                commit->merge(it->get());
                m_droppedCommits.push_back(std::move(*it));
                it = m_commits.erase(it);
            }
        }
    };
    mergeSequentials();
    if (m_commits.size() == 1) {
        // nothing to optimize anymore
        return;
    }
    if (m_commits.size() == 2 && m_commits.back()->areBuffersReadable()) {
        // special case: swapping two commits only requires one atomic test
        if (m_commits.back()->test()) {
            std::swap(m_commits.front(), m_commits.back());
        }
        return;
    }
    // move ready commits to the front, while testing that the new order doesn't break anything
    std::vector<std::unique_ptr<DrmAtomicCommit>> front;
    for (auto it = m_commits.begin() + 1; it != m_commits.end();) {
        auto &commit = *it;
        if (commit->areBuffersReadable()) {
            if (!commit->test()) {
                // if this doesn't already work, it's unlikely that newer commits will
                break;
            }
            // test if the new commit order would work
            bool success = true;
            auto duplicate = std::make_unique<DrmAtomicCommit>(*commit);
            for (const auto &otherCommit : m_commits) {
                if (otherCommit != commit) {
                    duplicate->merge(otherCommit.get());
                    if (!duplicate->test()) {
                        success = false;
                        break;
                    }
                }
            }
            if (success) {
                front.push_back(std::move(commit));
                it = m_commits.erase(it);
            } else {
                // if moving this one didn't work, it's unlikely that moving newer commits will
                break;
            }
            m_droppedCommits.push_back(std::move(duplicate));
        } else {
            it++;
        }
    }
    if (!front.empty()) {
        front.reserve(front.size() + m_commits.size());
        std::move(m_commits.begin(), m_commits.end(), std::back_inserter(front));
        m_commits = std::move(front);
        // there may be commits that can be merged now
        mergeSequentials();
    }
}

DrmCommitThread::~DrmCommitThread()
{
    m_thread->requestInterruption();
    m_commitPending.notify_all();
    m_thread->wait();
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
    m_commitPending.notify_all();
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
    if (m_lastPageflip > m_targetPageflipTime + m_minVblankInterval / 2) {
        m_missedPageflips++;
    }
    m_pageflipPending = false;
    if (!m_commits.empty()) {
        m_targetPageflipTime = estimateNextVblank(std::chrono::steady_clock::now());
        m_commitPending.notify_all();
    }
}

bool DrmCommitThread::pageflipsPending()
{
    std::unique_lock lock(m_mutex);
    return !m_commits.empty() || m_pageflipPending;
}

TimePoint DrmCommitThread::estimateNextVblank(TimePoint now) const
{
    const uint64_t pageflipsSince = (now - m_lastPageflip) / m_minVblankInterval;
    return m_lastPageflip + m_minVblankInterval * (pageflipsSince + 1);
}
}
