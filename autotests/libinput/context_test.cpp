/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_libinput.h"
#include "mock_udev.h"
#include "../../libinput/context.h"
#include "../../udev.h"
#include <QtTest>
Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtCriticalMsg)

using namespace KWin;
using namespace KWin::LibInput;

class TestContext : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void cleanup();
    void testCreateFailUdev();
    void testAssignSeat_data();
    void testAssignSeat();
};

void TestContext::cleanup()
{
    delete udev::s_mockUdev;
    udev::s_mockUdev = nullptr;
}

void TestContext::testCreateFailUdev()
{
    // this test verifies that isValid is false if the setup fails
    // we create an Udev without a mockUdev
    Udev u;
    QVERIFY(!(udev*)(u));
    Context context(u);
    QVERIFY(!context.isValid());
    // should not have a valid libinput
    libinput *libinput = context;
    QVERIFY(!libinput);
    QVERIFY(!context.assignSeat("testSeat"));
    QCOMPARE(context.fileDescriptor(), -1);
}

void TestContext::testAssignSeat_data()
{
    QTest::addColumn<bool>("assignShouldFail");
    QTest::addColumn<bool>("expectedValue");

    QTest::newRow("succeeds") << false << true;
    QTest::newRow("fails") << true << false;
}

void TestContext::testAssignSeat()
{
    // this test verifies the behavior of assignSeat
    // setup udev so that we can create a context
    udev::s_mockUdev = new udev;
    QVERIFY(udev::s_mockUdev);
    Udev u;
    QVERIFY((udev*)(u));
    Context context(u);
    QVERIFY(context.isValid());
    // this should give as a libinput
    libinput *libinput = context;
    QVERIFY(libinput);
    // and now we can assign it
    QFETCH(bool, assignShouldFail);
    libinput->assignSeatRetVal = assignShouldFail;
    QTEST(context.assignSeat("testSeat"), "expectedValue");
    // of course it's not suspended
    QVERIFY(!context.isSuspended());
}

QTEST_GUILESS_MAIN(TestContext)
#include "context_test.moc"
