/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_noop.h"

#include <fcntl.h>
#include <unistd.h>

namespace KWin
{

std::unique_ptr<NoopSession> NoopSession::create()
{
    return std::unique_ptr<NoopSession>{new NoopSession()};
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
    return ::open(fileName.toUtf8().data(), O_RDWR | O_CLOEXEC);
}

void NoopSession::closeRestricted(int fileDescriptor)
{
    ::close(fileDescriptor);
}

void NoopSession::switchTo(uint terminal)
{
}

FileDescriptor NoopSession::delaySleep(const QString &reason)
{
    return FileDescriptor{};
}

} // namespace KWin

#include "moc_session_noop.cpp"
