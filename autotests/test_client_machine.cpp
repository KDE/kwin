/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "testutils.h"
// KWin
#include "../client_machine.h"
#include "../xcbutils.h"
// Qt
#include <QApplication>
#include <QtTest/QtTest>
// xcb
#include <xcb/xcb.h>
// system
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

using namespace KWin;

class TestClientMachine : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void hostName_data();
    void hostName();
    void emptyHostName();

private:
    void setClientMachineProperty(xcb_window_t window, const QByteArray &hostname);
    QByteArray m_hostName;
    QByteArray m_fqdn;
};

void TestClientMachine::setClientMachineProperty(xcb_window_t window, const QByteArray &hostname)
{
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_CLIENT_MACHINE, XCB_ATOM_STRING, 8,
                        hostname.length(), hostname.constData());
}

void TestClientMachine::initTestCase()
{
#ifdef HOST_NAME_MAX
    char hostnamebuf[HOST_NAME_MAX];
#else
    char hostnamebuf[256];
#endif
    if (gethostname(hostnamebuf, sizeof hostnamebuf) >= 0) {
        hostnamebuf[sizeof(hostnamebuf)-1] = 0;
        m_hostName = hostnamebuf;
    }
    addrinfo *res;
    addrinfo addressHints;
    memset(&addressHints, 0, sizeof(addressHints));
    addressHints.ai_family = PF_UNSPEC;
    addressHints.ai_socktype = SOCK_STREAM;
    addressHints.ai_flags |= AI_CANONNAME;
    if (getaddrinfo(m_hostName.constData(), nullptr, &addressHints, &res) == 0) {
        if (res->ai_canonname) {
            m_fqdn = QByteArray(res->ai_canonname);
        }
    }
    freeaddrinfo(res);

    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestClientMachine::cleanupTestCase()
{
}

void TestClientMachine::hostName_data()
{
    QTest::addColumn<QByteArray>("hostName");
    QTest::addColumn<QByteArray>("expectedHost");
    QTest::addColumn<bool>("local");

    QTest::newRow("empty")     << QByteArray()            << QByteArray("localhost") << true;
    QTest::newRow("localhost") << QByteArray("localhost") << QByteArray("localhost") << true;
    QTest::newRow("hostname")  << m_hostName              << m_hostName              << true;
    QTest::newRow("HOSTNAME")  << m_hostName.toUpper()    << m_hostName.toUpper()    << true;
    QByteArray cutted(m_hostName);
    cutted.remove(0, 1);
    QTest::newRow("ostname")   << cutted << cutted << false;
    QByteArray domain("random.name.not.exist.tld");
    QTest::newRow("domain")    << domain << domain << false;
    QTest::newRow("fqdn")      << m_fqdn << m_fqdn << true;
    QTest::newRow("FQDN")      << m_fqdn.toUpper() << m_fqdn.toUpper() << true;
    cutted = m_fqdn;
    cutted.remove(0, 1);
    QTest::newRow("qdn")       << cutted << cutted << false;
}

void TestClientMachine::hostName()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = { true };
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    QFETCH(QByteArray, hostName);
    QFETCH(bool, local);
    setClientMachineProperty(window, hostName);

    ClientMachine clientMachine;
    QSignalSpy spy(&clientMachine, SIGNAL(localhostChanged()));
    clientMachine.resolve(window, XCB_WINDOW_NONE);
    QTEST(clientMachine.hostName(), "expectedHost");

    int i=0;
    while (clientMachine.isResolving() && i++ < 50) {
        // name is being resolved in an external thread, so let's wait a little bit
        QTest::qWait(250);
    }

    QCOMPARE(clientMachine.isLocal(), local);
    QCOMPARE(spy.isEmpty(), !local);
}

void TestClientMachine::emptyHostName()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = { true };
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    ClientMachine clientMachine;
    QSignalSpy spy(&clientMachine, SIGNAL(localhostChanged()));
    clientMachine.resolve(window, XCB_WINDOW_NONE);
    QCOMPARE(clientMachine.hostName(), ClientMachine::localhost());
    QVERIFY(clientMachine.isLocal());
    // should be local
    QCOMPARE(spy.isEmpty(), false);
}

QTEST_MAIN(TestClientMachine)
#include "test_client_machine.moc"
