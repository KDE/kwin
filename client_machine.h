/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_CLIENT_MACHINE_H
#define KWIN_CLIENT_MACHINE_H

#include <QObject>
#include <xcb/xcb.h>

// forward declaration
struct addrinfo;
template <typename T>
class QFutureWatcher;

namespace KWin {

class GetAddrInfo : public QObject
{
    Q_OBJECT
public:
    explicit GetAddrInfo(const QByteArray &hostName, QObject *parent = nullptr);
    ~GetAddrInfo() override;

    void resolve();

Q_SIGNALS:
    void local();

private Q_SLOTS:
    void slotResolved();
    void slotOwnAddressResolved();

private:
    void compare();
    bool resolved(QFutureWatcher<int> *watcher);
    bool m_resolving;
    bool m_resolved;
    bool m_ownResolved;
    QByteArray m_hostName;
    addrinfo *m_addressHints;
    addrinfo *m_address;
    addrinfo *m_ownAddress;
    QFutureWatcher<int> *m_watcher;
    QFutureWatcher<int> *m_ownAddressWatcher;
};

class ClientMachine : public QObject
{
    Q_OBJECT
public:
    explicit ClientMachine(QObject *parent = nullptr);
    ~ClientMachine() override;

    void resolve(xcb_window_t window, xcb_window_t clientLeader);
    const QByteArray &hostName() const;
    bool isLocal() const;
    static QByteArray localhost();
    bool isResolving() const;

Q_SIGNALS:
    void localhostChanged();

private Q_SLOTS:
    void setLocal();
    void resolveFinished();

private:
    void checkForLocalhost();
    QByteArray m_hostName;
    bool m_localhost;
    bool m_resolved;
    bool m_resolving;
};

inline
bool ClientMachine::isLocal() const
{
    return m_localhost;
}

inline
const QByteArray &ClientMachine::hostName() const
{
    return m_hostName;
}

inline
QByteArray ClientMachine::localhost()
{
    return "localhost";
}

inline
bool ClientMachine::isResolving() const
{
    return m_resolving;
}

} // namespace

#endif
