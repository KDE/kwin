/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif

#include "main.h"
#include "utils/common.h"

namespace KWin
{

class X11TestApplication : public Application
{
    Q_OBJECT
public:
    X11TestApplication(int &argc, char **argv);
    ~X11TestApplication() override;

protected:
    void performStartup() override;
};

X11TestApplication::X11TestApplication(int &argc, char **argv)
    : Application(OperationModeX11, argc, argv)
{
    setX11Connection(QX11Info::connection());
    setX11RootWindow(QX11Info::appRootWindow());
}

X11TestApplication::~X11TestApplication()
{
}

void X11TestApplication::performStartup()
{
}

}

class X11TimestampUpdateTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGrabAfterServerTime();
    void testBeforeLastGrabTime();
};

void X11TimestampUpdateTest::testGrabAfterServerTime()
{
    // this test tries to grab the X keyboard with a timestamp in future
    // that should fail, but after updating the X11 timestamp, it should
    // work again
    KWin::kwinApp()->updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();

    // now let's change the timestamp
    KWin::kwinApp()->setX11Time(KWin::xTime() + 5 * 60 * 1000);

    // now grab keyboard should fail
    QCOMPARE(KWin::grabXKeyboard(), false);

    // let's update timestamp, now it should work again
    KWin::kwinApp()->updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();
}

void X11TimestampUpdateTest::testBeforeLastGrabTime()
{
    // this test tries to grab the X keyboard with a timestamp before the
    // last grab time on the server. That should fail, but after updating the X11
    // timestamp it should work again

    // first set the grab timestamp
    KWin::kwinApp()->updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();

    // now go to past
    const auto timestamp = KWin::xTime();
    KWin::kwinApp()->setX11Time(KWin::xTime() - 5 * 60 * 1000, KWin::Application::TimestampUpdate::Always);
    QCOMPARE(KWin::xTime(), timestamp - 5 * 60 * 1000);

    // now grab keyboard should fail
    QCOMPARE(KWin::grabXKeyboard(), false);

    // let's update timestamp, now it should work again
    KWin::kwinApp()->updateXTime();
    QVERIFY(KWin::xTime() >= timestamp);
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();
}

int main(int argc, char *argv[])
{
    setenv("QT_QPA_PLATFORM", "xcb", true);
    KWin::X11TestApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    X11TimestampUpdateTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_x11_timestamp_update.moc"
