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

static const struct {
    Session::Type type;
    std::function<Session *(QObject *)> createFunc;
} s_availableSessions[] = {
    { Session::Type::Logind, &LogindSession::create },
    { Session::Type::ConsoleKit, &ConsoleKitSession::create },
    { Session::Type::Noop, &NoopSession::create },
};

Session::Session(QObject *parent)
    : QObject(parent)
{
}

Session *Session::create(QObject *parent)
{
    for (const auto &sessionInfo : s_availableSessions) {
        Session *session = sessionInfo.createFunc(parent);
        if (session) {
            return session;
        }
    }
    return nullptr;
}

Session *Session::create(Type type, QObject *parent)
{
    for (const auto &sessionInfo : s_availableSessions) {
        if (sessionInfo.type == type) {
            return sessionInfo.createFunc(parent);
        }
    }
    return nullptr;
}

} // namespace KWin
