/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "transaction.h"

#include <QObject>
#include <QSocketNotifier>

namespace KWin
{

class TransactionDmaBufLocker : public QObject
{
    Q_OBJECT

public:
    static TransactionDmaBufLocker *get(GraphicsBuffer *buffer);

    void add(Transaction *transaction);

private:
    explicit TransactionDmaBufLocker(const DmaBufAttributes *attributes);

    bool arm();

    QList<Transaction *> m_transactions;
    QList<QSocketNotifier *> m_pending;
    std::vector<std::unique_ptr<QSocketNotifier>> m_notifiers;
};

class TransactionEventFdLocker : public QObject
{
    Q_OBJECT
public:
    TransactionEventFdLocker(Transaction *transaction, FileDescriptor &&eventFd, ClientConnection *client);

private:
    void unlock();

    Transaction *const m_transaction;
    const QPointer<ClientConnection> m_client;
    const FileDescriptor m_eventFd;
    QSocketNotifier m_notifier;
};

} // namespace KWin
