/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"

#include <QPointer>

#include <functional>
#include <memory>
#include <vector>

namespace KWin
{

class SurfaceInterface;
struct SurfaceState;
class Transaction;

/**
 * The TransactionEntry type represents a log entry in a Transaction.
 */
struct TransactionEntry
{
    /**
     * The surface that is going to be affected by the transaction. Might be
     * \c null if the surface has been destroyed while the transaction is still
     * not ready.
     */
    QPointer<SurfaceInterface> surface;

    /**
     * The previous transaction that this transaction depends on.
     */
    Transaction *previousTransaction = nullptr;

    /**
     * Next transaction that is going to affect the surface.
     */
    Transaction *nextTransaction = nullptr;

    /**
     * Graphics buffer reference to prevent it from being destroyed.
     */
    GraphicsBufferRef buffer;

    /**
     * The surface state that is going to be applied.
     */
    std::unique_ptr<SurfaceState> state;
};

/**
 * The Transaction type provides a way to commit the pending state of one or more surfaces atomically.
 */
class KWIN_EXPORT Transaction
{
public:
    Transaction();

    /**
     * Locks the transaction. While the transaction is locked, it cannot be applied.
     */
    void lock();

    /**
     * Unlocks the transaction.
     */
    void unlock();

    /**
     * Returns \c true if this transaction can be applied, i.e. all its dependencies are resolved;
     * otherwise returns \c false.
     */
    bool isReady() const;

    /**
     * Returns the next transaction for the specified \a surface. If this transaction does
     * not affect the given surface, \c null is returned.
     */
    Transaction *next(SurfaceInterface *surface) const;

    /**
     * Adds the specified \a surface to this transaction. The transaction will move the pending
     * surface state and apply it when it's possible.
     */
    void add(SurfaceInterface *surface);

    /**
     * Amends already committed state.
     */
    void amend(SurfaceInterface *surface, std::function<void(SurfaceState *state)> mutator);

    /**
     * Merge the given \a other transaction with this transaction. The other transaction must be
     * uncommitted.
     */
    void merge(Transaction *other);

    /**
     * Commits the transaction. Note that the transaction may be applied later if there are previous
     * transactions that have not been applied yet or if the transaction is locked.
     *
     * The commit() function takes the ownership of the transaction. The transaction will be destroyed
     * when it is applied.
     */
    void commit();

private:
    void apply();
    bool tryApply();

    std::vector<TransactionEntry> m_entries;
    int m_locks = 0;
};

} // namespace KWin
