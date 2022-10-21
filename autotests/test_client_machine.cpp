/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "testutils.h"
// KWin
#include "client_machine.h"
#include "utils/xcbutils.h"
// Qt
#include <QApplication>
#include <QtTest>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
// xcb
#include <xcb/xcb.h>
// system
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
    void setClientMachineProperty(xcb_window_t window, const QString &hostname);
    QString m_hostName;
    QString m_fqdn;
};

void TestClientMachine::setClientMachineProperty(xcb_window_t window, const QString &hostname)
{
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_CLIENT_MACHINE, XCB_ATOM_STRING, 8,
                        hostname.length(), hostname.toLocal8Bit().constData());
}

void TestClientMachine::initTestCase()
{
#ifdef HOST_NAME_MAX
    char hostnamebuf[HOST_NAME_MAX];
#else
    char hostnamebuf[256];
#endif
    if (gethostname(hostnamebuf, sizeof hostnamebuf) >= 0) {
        hostnamebuf[sizeof(hostnamebuf) - 1] = 0;
        m_hostName = hostnamebuf;
    }
    addrinfo *res;
    addrinfo addressHints;
    memset(&addressHints, 0, sizeof(addressHints));
    addressHints.ai_family = PF_UNSPEC;
    addressHints.ai_socktype = SOCK_STREAM;
    addressHints.ai_flags |= AI_CANONNAME;
    if (getaddrinfo(m_hostName.toLocal8Bit().constData(), nullptr, &addressHints, &res) == 0) {
        if (res->ai_canonname) {
            m_fqdn = QString::fromLocal8Bit(res->ai_canonname);
        }
    }
    freeaddrinfo(res);

    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void *>(QX11Info::connection()));
}

void TestClientMachine::cleanupTestCase()
{
}

void TestClientMachine::hostName_data()
{
    QTest::addColumn<QString>("hostName");
    QTest::addColumn<QString>("expectedHost");
    QTest::addColumn<bool>("local");

    QTest::newRow("empty") << QString() << QStringLiteral("localhost") << true;
    QTest::newRow("localhost") << QStringLiteral("localhost") << QStringLiteral("localhost") << true;
    QTest::newRow("hostname") << m_hostName << m_hostName << true;
    QTest::newRow("HOSTNAME") << m_hostName.toUpper() << m_hostName.toUpper() << true;
    QString cutted(m_hostName);
    cutted.remove(0, 1);
    QTest::newRow("ostname") << cutted << cutted << false;
    QString domain("random.name.not.exist.tld");
    QTest::newRow("domain") << domain << domain << false;
    QTest::newRow("fqdn") << m_fqdn << m_fqdn << true;
    QTest::newRow("FQDN") << m_fqdn.toUpper() << m_fqdn.toUpper() << true;
    cutted = m_fqdn;
    cutted.remove(0, 1);
    QTest::newRow("qdn") << cutted << cutted << false;
}

void TestClientMachine::hostName()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    QFETCH(QString, hostName);
    QFETCH(bool, local);
    setClientMachineProperty(window, hostName);

    ClientMachine clientMachine;
    QSignalSpy spy(&clientMachine, &ClientMachine::localhostChanged);
    clientMachine.resolve(window, XCB_WINDOW_NONE);
    QTEST(clientMachine.hostName(), "expectedHost");

    int i = 0;
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
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    ClientMachine clientMachine;
    QSignalSpy spy(&clientMachine, &ClientMachine::localhostChanged);
    clientMachine.resolve(window, XCB_WINDOW_NONE);
    QCOMPARE(clientMachine.hostName(), ClientMachine::localhost());
    QVERIFY(clientMachine.isLocal());
    // should be local
    QCOMPARE(spy.isEmpty(), false);
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestClientMachine)
#include "test_client_machine.moc"
