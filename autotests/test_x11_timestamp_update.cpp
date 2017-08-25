/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

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

#include <QTest>
#include <QX11Info>

#include <KPluginMetaData>

#include "main.h"
#include "utils.h"

namespace KWin
{

class X11TestApplication : public Application
{
    Q_OBJECT
public:
    X11TestApplication(int &argc, char **argv);
    virtual ~X11TestApplication();

protected:
    void performStartup() override;

};

X11TestApplication::X11TestApplication(int &argc, char **argv)
    : Application(OperationModeX11, argc, argv)
{
    setX11Connection(QX11Info::connection());
    setX11RootWindow(QX11Info::appRootWindow());
    initPlatform(KPluginMetaData(QStringLiteral("KWinX11Platform.so")));
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
    KWin::updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();

    // now let's change the timestamp
    KWin::kwinApp()->setX11Time(KWin::xTime() + 5 * 60 * 1000);

    // now grab keyboard should fail
    QCOMPARE(KWin::grabXKeyboard(), false);

    // let's update timestamp, now it should work again
    KWin::updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();
}

void X11TimestampUpdateTest::testBeforeLastGrabTime()
{
    // this test tries to grab the X keyboard with a timestamp before the
    // last grab time on the server. That should fail, but after updating the X11
    // timestamp it should work again

    // first set the grab timestamp
    KWin::updateXTime();
    QCOMPARE(KWin::grabXKeyboard(), true);
    KWin::ungrabXKeyboard();

    // now go to past
    const auto timestamp = KWin::xTime();
    KWin::kwinApp()->setX11Time(KWin::xTime() - 5 * 60 * 1000, KWin::Application::TimestampUpdate::Always);
    QCOMPARE(KWin::xTime(), timestamp - 5 * 60 * 1000);

    // now grab keyboard should fail
    QCOMPARE(KWin::grabXKeyboard(), false);

    // let's update timestamp, now it should work again
    KWin::updateXTime();
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
