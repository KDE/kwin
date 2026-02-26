/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtTest>

namespace KWin
{

template<typename Signal>
struct SignalPointer
{
};

template<typename ReturnType, typename ObjectType, typename... Arguments>
struct SignalPointer<ReturnType (ObjectType::*)(Arguments...)>
{
    typedef ObjectType Object;
};

template<typename ReturnType, typename ObjectType, typename... Arguments>
struct SignalPointer<ReturnType (ObjectType::*)(Arguments...) const>
{
    typedef ObjectType Object;
};

class SignalSpy : public QList<QList<QVariant>>
{
public:
    template<typename Signal>
    SignalSpy(const typename SignalPointer<Signal>::Object *object, Signal signal)
        : m_receiver(std::make_unique<QObject>())
    {
        QObject::connect(object, signal, m_receiver.get(), [this](auto... arguments) {
            append(QList<QVariant>{QVariant::fromValue(arguments)...});
            if (m_waiting) {
                m_eventLoop.exitLoop();
            }
        });
    }

    bool wait()
    {
        static std::chrono::seconds timeout(qEnvironmentVariableIntegerValue("KSIGNALSPY_WAIT_TIMEOUT").value_or(5));
        return wait(timeout);
    }

    bool wait(int timeout)
    {
        return wait(std::chrono::milliseconds(timeout));
    }

    bool wait(std::chrono::milliseconds timeout)
    {
        Q_ASSERT(!m_waiting);

        const int originalSize = size();

        m_waiting = true;
        m_eventLoop.enterLoop(timeout);

        m_waiting = false;
        return originalSize < size();
    }

private:
    std::unique_ptr<QObject> m_receiver;
    QTestEventLoop m_eventLoop;
    bool m_waiting = false;
};

} // namespace KWin
