/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_noop.h"

namespace KWin
{

NoopSession *NoopSession::create(QObject *parent)
{
    return new NoopSession(parent);
}

NoopSession::NoopSession(QObject *parent)
    : Session(parent)
{
}

NoopSession::~NoopSession()
{
}

bool NoopSession::isActive() const
{
    return true;
}

NoopSession::Capabilities NoopSession::capabilities() const
{
    return Capabilities();
}

QString NoopSession::seat() const
{
    return QStringLiteral("seat0");
}

uint NoopSession::terminal() const
{
    return 0;
}

int NoopSession::openRestricted(const QString &fileName)
{
    Q_UNUSED(fileName)
    return -1;
}

void NoopSession::closeRestricted(int fileDescriptor)
{
    Q_UNUSED(fileDescriptor)
}

void NoopSession::switchTo(uint terminal)
{
    Q_UNUSED(terminal)
}

} // namespace KWin
