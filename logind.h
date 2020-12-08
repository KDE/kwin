/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_LOGIND_H
#define KWIN_LOGIND_H

#include <kwinglobals.h>

#include <QDBusConnection>
#include <QObject>

class QDBusServiceWatcher;

namespace KWin
{

class KWIN_EXPORT LogindIntegration : public QObject
{
    Q_OBJECT
public:
    ~LogindIntegration() override;

    bool isConnected() const {
        return m_connected;
    }
    bool hasSessionControl() const {
        return m_sessionControl;
    }
    bool isActiveSession() const {
        return m_sessionActive;
    }
    int vt() const {
        return m_vt;
    }
    void switchVirtualTerminal(quint32 vtNr);

    void takeControl();
    void releaseControl();

    int takeDevice(const char *path);
    void releaseDevice(int fd);

    const QString seat() const {
        return m_seatName;
    }

Q_SIGNALS:
    void connectedChanged();
    void hasSessionControlChanged(bool);
    void sessionActiveChanged(bool);
    void virtualTerminalChanged(int);
    void prepareForSleep(bool prepare);

private Q_SLOTS:
    void getSessionActive();
    void getVirtualTerminal();
    void pauseDevice(uint major, uint minor, const QString &type);

private:
    friend class LogindTest;
    /**
     * The DBusConnection argument is needed for the unit test. Logind uses the system bus
     * on which the unit test's fake logind cannot register to. Thus the unit test need to
     * be able to do everything over the session bus. This ctor allows the LogindTest to
     * create a LogindIntegration which listens on the session bus.
     */
    explicit LogindIntegration(const QDBusConnection &connection, QObject *parent = nullptr);
    void logindServiceRegistered();
    void connectSessionPropertiesChanged();
    enum SessionController {
        SessionControllerLogind,
        SessionControllerConsoleKit,
    };
    void setupSessionController(SessionController controller);
    void getSeat();
    QDBusConnection m_bus;
    QDBusServiceWatcher *m_logindServiceWatcher;
    bool m_connected;
    QString m_sessionPath;
    bool m_sessionControl;
    bool m_sessionActive;
    int m_vt = -1;
    QString m_seatName = QStringLiteral("seat0");
    QString m_seatPath;
    QString m_sessionControllerName;
    QString m_sessionControllerService;
    QString m_sessionControllerPath;
    QString m_sessionControllerManagerInterface;
    QString m_sessionControllerSeatInterface;
    QString m_sessionControllerSessionInterface;
    QString m_sessionControllerActiveProperty;
    KWIN_SINGLETON(LogindIntegration)
};

}

#endif
