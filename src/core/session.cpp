/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session.h"
#include "session_consolekit.h"
#include "session_logind.h"
#include "session_noop.h"

namespace KWin
{

static const struct
{
    Session::Type type;
    std::function<std::unique_ptr<Session>()> createFunc;
} s_availableSessions[] = {
    {Session::Type::Logind, &LogindSession::create},
    {Session::Type::ConsoleKit, &ConsoleKitSession::create},
    {Session::Type::Noop, &NoopSession::create},
};

std::unique_ptr<Session> Session::create()
{
    for (const auto &sessionInfo : s_availableSessions) {
        std::unique_ptr<Session> session = sessionInfo.createFunc();
        if (session) {
            return session;
        }
    }
    return nullptr;
}

std::unique_ptr<Session> Session::create(Type type)
{
    for (const auto &sessionInfo : s_availableSessions) {
        if (sessionInfo.type == type) {
            return sessionInfo.createFunc();
        }
    }
    return nullptr;
}

} // namespace KWin

#include "moc_session.cpp"
