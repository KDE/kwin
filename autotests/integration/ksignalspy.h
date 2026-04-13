/*
 SPDX-FileCopyrightText: 2026 David Edmundson <davidedmundson@kde.org>
 SPDX-License-Identifier: LGPL-2.0-or-later
*/

// I don't know why this doesn't exist in Qt already, intention is to test and then upstream

#include <qtesteventloop.h>
#include <tuple>
#include <QList>
#include <QMutex>
#include <QObject>

/** A version of QSignalSpy with types
 **/

template< class... Types >
class KSignalSpy : public QList<std::tuple<Types...>>
{
public:
    template <typename Func1>
    KSignalSpy(const typename QtPrivate::FunctionPointer<Func1>::Object *sender, Func1 signal) {
        m_connection = QObject::connect(sender, signal, sender, [this](Types... args) {
            QMutexLocker locker(&m_mutex);
            this->append(std::make_tuple(args...));
            if (m_waiting) {
                locker.unlock();
                m_loop.exitLoop();
            }
        }, Qt::DirectConnection);
    }

    ~KSignalSpy() {
        QObject::disconnect(m_connection);
    }

    bool wait(std::chrono::milliseconds timeout = std::chrono::seconds{5}) {
        QMutexLocker locker(&m_mutex);
        Q_ASSERT(!m_waiting);
        const qsizetype origCount = QList<std::tuple<Types...>>::size();
        m_waiting = true;
        locker.unlock();

        m_loop.enterLoop(timeout);

        locker.relock();
        m_waiting = false;
        return QList<std::tuple<Types...>>::size() > origCount;
    }

    bool wait(int timeoutInSeconds)
    {
        return wait(std::chrono::seconds{timeoutInSeconds});
    }

    // for compatibility of porting
    bool isValid() {
        return true;
    }

private:
    QMetaObject::Connection m_connection;
    bool m_waiting = false;
    QTestEventLoop m_loop;
    QMutex m_mutex;
};

template<typename Object, typename... Types>
KSignalSpy(const Object *sender, void (Object::*)(Types...)) -> KSignalSpy<std::decay_t<Types>...>;

template<typename Object, typename... Types>
KSignalSpy(const Object *sender, void (Object::*)(Types...) const) -> KSignalSpy<std::decay_t<Types>...>;
