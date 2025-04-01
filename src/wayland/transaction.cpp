/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/transaction.h"
#include "core/syncobjtimeline.h"
#include "utils/filedescriptor.h"
#include "wayland/clientconnection.h"
#include "wayland/subcompositor.h"
#include "wayland/surface_p.h"

#if defined(Q_OS_LINUX)
#include <linux/dma-buf.h>
#include <xf86drm.h>
#endif

namespace KWin
{

TransactionFence::TransactionFence(Transaction *transaction, FileDescriptor &&fileDescriptor)
    : m_transaction(transaction)
    , m_fileDescriptor(std::move(fileDescriptor))
{
    m_notifier = std::make_unique<QSocketNotifier>(m_fileDescriptor.get(), QSocketNotifier::Read);
    QObject::connect(m_notifier.get(), &QSocketNotifier::activated, [this]() {
        m_notifier->setEnabled(false);
        m_transaction->tryApply();
    });
}

bool TransactionFence::isWaiting() const
{
    return m_notifier->isEnabled();
}

bool TransactionEntry::isDiscarded() const
{
    return !surface || surface->tearingDown() || surface->client()->tearingDown();
}

Transaction::Transaction()
{
}

bool Transaction::isReady() const
{
    return std::none_of(m_entries.cbegin(), m_entries.cend(), [](const TransactionEntry &entry) {
        if (entry.previousTransaction) {
            return true;
        }

        if (entry.isDiscarded()) {
            return false;
        }

        for (const auto &fence : entry.fences) {
            if (fence->isWaiting()) {
                return true;
            }
        }

        return entry.state->hasFifoWaitCondition && entry.surface->hasFifoBarrier();
    });
}

Transaction *Transaction::next(SurfaceInterface *surface) const
{
    for (const TransactionEntry &entry : m_entries) {
        if (entry.surface == surface) {
            return entry.nextTransaction;
        }
    }
    return nullptr;
}

void Transaction::add(SurfaceInterface *surface)
{
    SurfaceState *pending = SurfaceInterfacePrivate::get(surface)->pending.get();

    for (TransactionEntry &entry : m_entries) {
        if (entry.surface == surface) {
            if (pending->committed & SurfaceState::Field::Buffer) {
                entry.buffer = GraphicsBufferRef(pending->buffer);
            }
            pending->mergeInto(entry.state.get());
            return;
        }
    }

    auto state = std::make_unique<SurfaceState>();
    pending->mergeInto(state.get());

    m_entries.emplace_back(TransactionEntry{
        .surface = surface,
        .buffer = GraphicsBufferRef(state->buffer),
        .state = std::move(state),
    });
}

void Transaction::amend(SurfaceInterface *surface, std::function<void(SurfaceState *)> mutator)
{
    for (TransactionEntry &entry : m_entries) {
        if (entry.surface == surface) {
            mutator(entry.state.get());
        }
    }
}

void Transaction::merge(Transaction *other)
{
    for (size_t i = 0; i < other->m_entries.size(); ++i) {
        m_entries.emplace_back(std::move(other->m_entries[i]));
    }
    other->m_entries.clear();
}

static bool isAncestor(SurfaceInterface *surface, SurfaceInterface *ancestor)
{
    SurfaceInterface *candidate = surface;
    while (candidate) {
        SubSurfaceInterface *subsurface = candidate->subSurface();
        if (!subsurface) {
            return false;
        }

        if (subsurface->parentSurface() == ancestor) {
            return true;
        }

        candidate = subsurface->parentSurface();
    }

    return false;
}

static SurfaceInterface *mainSurface(SurfaceInterface *surface)
{
    SubSurfaceInterface *subsurface = surface->subSurface();
    if (subsurface) {
        return subsurface->mainSurface();
    }
    return surface;
}

void Transaction::apply()
{
    // Sort surfaces so descendants come first, then their ancestors.
    std::sort(m_entries.begin(), m_entries.end(), [](const TransactionEntry &a, const TransactionEntry &b) {
        if (!a.surface) {
            return false;
        }
        if (!b.surface) {
            return true;
        }

        if (isAncestor(a.surface, b.surface)) {
            return true;
        }
        if (isAncestor(b.surface, a.surface)) {
            return false;
        }
        return mainSurface(a.surface) < mainSurface(b.surface);
    });

    for (TransactionEntry &entry : m_entries) {
        if (!entry.isDiscarded()) {
            SurfaceInterfacePrivate::get(entry.surface)->applyState(entry.state.get());
        }
    }

    for (TransactionEntry &entry : m_entries) {
        if (entry.surface) {
            if (entry.surface->lastTransaction() == this) {
                entry.surface->setFirstTransaction(nullptr);
                entry.surface->setLastTransaction(nullptr);
            } else {
                entry.surface->setFirstTransaction(entry.nextTransaction);
            }
        }

        if (entry.nextTransaction) {
            for (TransactionEntry &otherEntry : entry.nextTransaction->m_entries) {
                if (otherEntry.previousTransaction == this) {
                    otherEntry.previousTransaction = nullptr;
                    break;
                }
            }
            entry.nextTransaction->tryApply();
        }
    }

    delete this;
}

void Transaction::tryApply()
{
    if (isReady()) {
        apply();
    }
}

void Transaction::commit()
{
    for (TransactionEntry &entry : m_entries) {
        if (!entry.surface) {
            continue;
        }

        if ((entry.state->committed & SurfaceState::Field::Buffer) && entry.state->buffer) {
            // Avoid applying the transaction until all graphics buffers have become idle.
            if (entry.state->acquirePoint.timeline) {
                watchSyncObj(&entry);
            } else {
                watchDmaBuf(&entry);
            }
        }

        if (entry.surface->firstTransaction()) {
            Transaction *lastTransaction = entry.surface->lastTransaction();
            for (TransactionEntry &lastEntry : lastTransaction->m_entries) {
                if (lastEntry.surface == entry.surface) {
                    lastEntry.nextTransaction = this;
                }
            }
        } else {
            entry.surface->setFirstTransaction(this);
        }

        entry.previousTransaction = entry.surface->lastTransaction();
        entry.surface->setLastTransaction(this);
    }

    tryApply();
}

void Transaction::watchSyncObj(TransactionEntry *entry)
{
    auto eventFd = entry->state->acquirePoint.timeline->eventFd(entry->state->acquirePoint.point);
    if (!eventFd.isValid()) {
        return;
    }

    if (eventFd.isReadable()) {
        return;
    }

    entry->fences.emplace_back(std::make_unique<TransactionFence>(this, std::move(eventFd)));
}

#if defined(Q_OS_LINUX)
static FileDescriptor exportWaitSyncFile(const FileDescriptor &fileDescriptor)
{
    dma_buf_export_sync_file request{
        .flags = DMA_BUF_SYNC_READ,
        .fd = -1,
    };
    if (drmIoctl(fileDescriptor.get(), DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) == 0) {
        return FileDescriptor(request.fd);
    }

    return FileDescriptor{};
}
#endif

void Transaction::watchDmaBuf(TransactionEntry *entry)
{
#if defined(Q_OS_LINUX)
    const DmaBufAttributes *attributes = entry->buffer->dmabufAttributes();
    if (!attributes) {
        return;
    }

    for (int i = 0; i < attributes->planeCount; ++i) {
        const FileDescriptor &fileDescriptor = attributes->fd[i];
        if (fileDescriptor.isReadable()) {
            continue;
        }

        auto syncFile = exportWaitSyncFile(fileDescriptor);
        if (syncFile.isValid()) {
            entry->fences.emplace_back(std::make_unique<TransactionFence>(this, std::move(syncFile)));
        }
    }
#endif
}

} // namespace KWin
