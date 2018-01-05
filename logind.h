/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
    ~LogindIntegration();

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
     **/
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
