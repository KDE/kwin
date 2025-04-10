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
#include "utils/envvar.h"
#include "utils/realtime.h"

#include <ranges>
#include <span>
#include <thread>

using namespace std::chrono_literals;

namespace KWin
{

DrmCommitThread::DrmCommitThread(DrmGpu *gpu, const QString &name)
    : m_gpu(gpu)
    , m_targetPageflipTime(std::chrono::steady_clock::now())
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
                timeout = m_commitPending.wait_for(lock, DrmGpu::s_pageflipTimeout) == std::cv_status::timeout;
            } else if (m_commits.empty()) {
                m_commitPending.wait(lock);
            }
            if (m_committed) {
                if (timeout) {
                    // if the main thread just hung for a while, the pageflip will be processed after the wait
                    // but not if it's a real pageflip timeout
                    m_ping = false;
                    QMetaObject::invokeMethod(this, &DrmCommitThread::handlePing, Qt::ConnectionType::QueuedConnection);
                    while (!m_ping) {
                        m_pong.wait(lock);
                    }
                    if (m_committed) {
                        qCCritical(KWIN_DRM, "Pageflip timed out! This is a bug in the %s kernel driver", qPrintable(m_gpu->driverName()));
                        if (m_gpu->isAmdgpu()) {
                            qCCritical(KWIN_DRM, "Please report this at https://gitlab.freedesktop.org/drm/amd/-/issues");
                        } else if (m_gpu->isNVidia()) {
                            qCCritical(KWIN_DRM, "Please report this at https://forums.developer.nvidia.com/c/gpu-graphics/linux");
                        } else if (m_gpu->isI915()) {
                            qCCritical(KWIN_DRM, "Please report this at https://gitlab.freedesktop.org/drm/i915/kernel/-/issues");
                        }
                        qCCritical(KWIN_DRM, "With the output of 'sudo dmesg' and 'journalctl --user-unit plasma-kwin_wayland --boot 0'");
                        m_pageflipTimeoutDetected = true;
                    } else {
                        qCWarning(KWIN_DRM, "The main thread was hanging temporarily!");
                    }
                } else {
                    // the commit would fail with EBUSY, wait until the pageflip is done
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
                // the main thread might've modified the list
                if (m_commits.empty()) {
                    continue;
                }
            }
            optimizeCommits(m_targetPageflipTime);
            if (!m_commits.front()->isReadyFor(m_targetPageflipTime)) {
                // no commit is ready yet, reschedule
                if (m_vrr || m_tearing) {
                    m_targetPageflipTime += 50us;
                } else {
                    m_targetPageflipTime += m_minVblankInterval;
                }
                continue;
            }
            if (m_commits.front()->allowedVrrDelay() && m_vrr) {
                // wait for a higher priority commit to be in, or the timeout to be hit
                const bool allDelay = std::ranges::all_of(m_commits, [](const auto &commit) {
                    return commit->allowedVrrDelay().has_value();
                });
                auto delays = m_commits | std::views::filter([](const auto &commit) {
                    return commit->allowedVrrDelay().has_value();
                }) | std::views::transform([](const auto &commit) {
                    return *commit->allowedVrrDelay();
                });
                const std::chrono::nanoseconds lowestDelay = *std::ranges::min_element(delays);
                const auto delayedTarget = m_lastPageflip + lowestDelay;
                if (allDelay) {
                    // all commits should be delayed, just wait for the timeout
                    if (m_commitPending.wait_until(lock, delayedTarget) == std::cv_status::no_timeout) {
                        continue;
                    }
                } else {
                    // TODO replace this with polling for the buffers to be ready instead
                    bool timeout = true;
                    while (std::chrono::steady_clock::now() < delayedTarget && timeout && m_commits.front()->allowedVrrDelay().has_value()) {
                        timeout = m_commitPending.wait_for(lock, 50us) == std::cv_status::timeout;
                        if (m_commits.empty()) {
                            break;
                        }
                        optimizeCommits(delayedTarget);
                    }
                    if (!timeout) {
                        // some new commit was added, process that
                        continue;
                    }
                }
                if (m_commits.empty()) {
                    continue;
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
    DrmAtomicCommit *commit = m_commits.front().get();
    const auto vrr = commit->isVrr();
    const bool success = commit->commit();
    if (success) {
        m_vrr = vrr.value_or(m_vrr);
        m_tearing = commit->isTearing();
        m_committed = std::move(m_commits.front());
        m_commits.erase(m_commits.begin());

        // the kernel might still take some time to actually apply the commit
        // after we return from the commit ioctl, but we don't have any better
        // way to know when it's done
        m_lastCommitTime = std::chrono::steady_clock::now();
        // this is when we wanted to have completed the commit
        const auto targetTimestamp = m_targetPageflipTime - m_baseSafetyMargin;
        // this is how much safety we need to add or remove to achieve that next time
        const auto safetyDifference = targetTimestamp - m_lastCommitTime;
        if (safetyDifference < std::chrono::nanoseconds::zero()) {
            // the commit was done later than desired, immediately add the
            // required difference to make sure that it doesn't happen again
            m_additionalSafetyMargin -= safetyDifference;
        } else {
            // we were done earlier than desired. This isn't problematic, but
            // we want to keep latency at a minimum, so slowly reduce the safety margin
            m_additionalSafetyMargin -= safetyDifference / 10;
        }
        const auto maximumReasonableMargin = std::min<std::chrono::nanoseconds>(3ms, m_minVblankInterval / 2);
        m_additionalSafetyMargin = std::clamp(m_additionalSafetyMargin, 0ns, maximumReasonableMargin);
        m_safetyMargin = m_baseSafetyMargin + m_additionalSafetyMargin;
    } else {
        if (m_commits.size() > 1) {
            // the failure may have been because of the reordering of commits
            // -> collapse all commits into one and try again with an already tested state
            while (m_commits.size() > 1) {
                auto toMerge = std::move(m_commits[1]);
                m_commits.erase(m_commits.begin() + 1);
                commit->merge(toMerge.get());
                m_commitsToDelete.push_back(std::move(toMerge));
            }
            if (commit->test()) {
                // presentation didn't fail after all, try again
                submit();
                return;
            }
        }
        for (auto &commit : m_commits) {
            m_commitsToDelete.push_back(std::move(commit));
        }
        m_commits.clear();
        qCWarning(KWIN_DRM) << "atomic commit failed:" << strerror(errno);
    }
    QMetaObject::invokeMethod(this, &DrmCommitThread::clearDroppedCommits, Qt::ConnectionType::QueuedConnection);
}

static std::unique_ptr<DrmAtomicCommit> mergeCommits(std::span<const std::unique_ptr<DrmAtomicCommit>> commits)
{
    auto ret = std::make_unique<DrmAtomicCommit>(*commits.front());
    for (const auto &onTop : commits.subspan(1)) {
        ret->merge(onTop.get());
    }
    return ret;
}

void DrmCommitThread::optimizeCommits(TimePoint pageflipTarget)
{
    if (m_commits.size() <= 1) {
        return;
    }
    // merge commits in the front that are already ready (regardless of which planes they modify)
    if (m_commits.front()->areBuffersReadable()) {
        const auto firstNotReady = std::find_if(m_commits.begin() + 1, m_commits.end(), [pageflipTarget](const auto &commit) {
            return !commit->isReadyFor(pageflipTarget);
        });
        if (firstNotReady != m_commits.begin() + 1) {
            auto merged = mergeCommits(std::span(m_commits.begin(), firstNotReady));
            std::move(m_commits.begin(), firstNotReady, std::back_inserter(m_commitsToDelete));
            m_commits.erase(m_commits.begin() + 1, firstNotReady);
            m_commits.front() = std::move(merged);
        }
    }
    // merge commits that are ready and modify the same drm planes
    for (auto it = m_commits.begin(); it != m_commits.end();) {
        const auto startIt = it;
        auto &startCommit = *startIt;
        const auto firstNotSamePlaneNotReady = std::find_if(startIt + 1, m_commits.end(), [&startCommit, pageflipTarget](const auto &commit) {
            return startCommit->modifiedPlanes() != commit->modifiedPlanes() || !commit->isReadyFor(pageflipTarget);
        });
        if (firstNotSamePlaneNotReady == startIt + 1) {
            it++;
            continue;
        }
        auto merged = mergeCommits(std::span(startIt, firstNotSamePlaneNotReady));
        std::move(startIt, firstNotSamePlaneNotReady, std::back_inserter(m_commitsToDelete));
        startCommit = std::move(merged);
        it = m_commits.erase(startIt + 1, firstNotSamePlaneNotReady);
    }
    if (m_commits.size() == 1) {
        // already done
        return;
    }
    std::unique_ptr<DrmAtomicCommit> front;
    if (m_commits.front()->isReadyFor(pageflipTarget)) {
        // can't just move the commit, or merging might drop the last reference
        // to an OutputFrame, which should only happen in the main thread
        front = std::make_unique<DrmAtomicCommit>(*m_commits.front());
        m_commitsToDelete.push_back(std::move(m_commits.front()));
        m_commits.erase(m_commits.begin());
    }
    // try to move commits that are ready to the front
    for (auto it = m_commits.begin() + 1; it != m_commits.end();) {
        auto &commit = *it;
        if (!commit->isReadyFor(pageflipTarget)) {
            it++;
            continue;
        }
        // commits that target the same plane(s) need to stay in the same order
        const auto &planes = commit->modifiedPlanes();
        const bool skipping = std::any_of(m_commits.begin(), it, [&planes](const auto &other) {
            return std::ranges::any_of(planes, [&other](DrmPlane *plane) {
                return other->modifiedPlanes().contains(plane);
            });
        });
        if (skipping) {
            it++;
            continue;
        }
        // find out if the modified commit order will actually work
        std::unique_ptr<DrmAtomicCommit> duplicate;
        if (front) {
            duplicate = std::make_unique<DrmAtomicCommit>(*front);
            duplicate->merge(commit.get());
            if (!duplicate->test()) {
                m_commitsToDelete.push_back(std::move(duplicate));
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
        m_commitsToDelete.push_back(std::move(duplicate));
        if (success) {
            if (front) {
                front->merge(commit.get());
                m_commitsToDelete.push_back(std::move(commit));
            } else {
                front = std::make_unique<DrmAtomicCommit>(*commit);
                m_commitsToDelete.push_back(std::move(commit));
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
        {
            std::unique_lock lock(m_mutex);
            m_thread->requestInterruption();
            m_commitPending.notify_all();
            m_ping = true;
            m_pong.notify_all();
        }
        m_thread->wait();
    }
    if (m_committed) {
        m_committed->setDefunct();
        m_gpu->addDefunctCommit(std::move(m_committed));
    }
}

void DrmCommitThread::addCommit(std::unique_ptr<DrmAtomicCommit> &&commit)
{
    std::unique_lock lock(m_mutex);
    m_commits.push_back(std::move(commit));
    const auto now = std::chrono::steady_clock::now();
    TimePoint newTarget;
    if (m_tearing) {
        newTarget = now;
    } else if (m_vrr && now >= m_lastPageflip + m_minVblankInterval) {
        newTarget = now;
    } else {
        newTarget = estimateNextVblank(now);
    }
    m_targetPageflipTime = std::max(m_targetPageflipTime, newTarget);
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
    m_commitsToDelete.clear();
}

// TODO reduce the default for this, once we have a more accurate way to know when an atomic commit
// is actually applied. Waiting for the commit returning seems to work on Intel and AMD, but not with NVidia
static const std::chrono::microseconds s_safetyMarginMinimum{environmentVariableIntValue("KWIN_DRM_OVERRIDE_SAFETY_MARGIN").value_or(1500)};

void DrmCommitThread::setModeInfo(uint32_t maximum, std::chrono::nanoseconds vblankTime)
{
    std::unique_lock lock(m_mutex);
    m_minVblankInterval = std::chrono::nanoseconds(1'000'000'000'000ull / maximum);
    // the kernel rejects commits that happen during vblank
    // the 1.5ms on top of that was chosen experimentally, for the time it takes to commit + scheduling inaccuracies
    m_baseSafetyMargin = vblankTime + s_safetyMarginMinimum;
    m_safetyMargin = m_baseSafetyMargin + m_additionalSafetyMargin;
}

void DrmCommitThread::pageFlipped(std::chrono::nanoseconds timestamp)
{
    std::unique_lock lock(m_mutex);
    if (m_pageflipTimeoutDetected) {
        qCCritical(KWIN_DRM, "Pageflip arrived after all, %lums after the commit", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastCommitTime).count());
        m_pageflipTimeoutDetected = false;
    }
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

void DrmCommitThread::handlePing()
{
    // this will process the pageflip and call pageFlipped if there is one
    m_gpu->dispatchEvents();
    std::unique_lock lock(m_mutex);
    m_ping = true;
    m_pong.notify_one();
}
}
