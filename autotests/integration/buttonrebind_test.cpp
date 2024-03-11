/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "pointer_input.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>
#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_buttonrebind-0");

class TestButtonRebind : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testKey_data();
    void testKey();

private:
    quint32 timestamp = 1;
};

void TestButtonRebind::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void TestButtonRebind::cleanup()
{
    Test::destroyWaylandConnection();
    QVERIFY(QFile::remove(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kcminputrc"))));
}

void TestButtonRebind::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void TestButtonRebind::testKey_data()
{
    QTest::addColumn<QKeySequence>("boundKeys");
    QTest::addColumn<QList<quint32>>("expectedKeys");

    QTest::newRow("single key") << QKeySequence(Qt::Key_A) << QList<quint32>{KEY_A};
    QTest::newRow("single modifier") << QKeySequence(Qt::Key_Control) << QList<quint32>{KEY_LEFTCTRL};
    QTest::newRow("single modifier plus key") << QKeySequence(Qt::ControlModifier | Qt::Key_N) << QList<quint32>{KEY_LEFTCTRL, KEY_N};
    QTest::newRow("multiple modifiers plus key") << QKeySequence(Qt::ControlModifier | Qt::MetaModifier | Qt::Key_Y) << QList<quint32>{KEY_LEFTCTRL, KEY_LEFTMETA, KEY_Y};
    QTest::newRow("delete") << QKeySequence(Qt::Key_Delete) << QList<quint32>{KEY_DELETE};
    QTest::newRow("keypad delete") << QKeySequence(Qt::KeypadModifier | Qt::Key_Delete) << QList<quint32>{KEY_KPDOT};
    QTest::newRow("keypad enter") << QKeySequence(Qt::KeypadModifier | Qt::Key_Enter) << QList<quint32>{KEY_KPENTER};
}

void TestButtonRebind::testKey()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("Mouse"));
    QFETCH(QKeySequence, boundKeys);
    buttonGroup.writeEntry("ExtraButton7", QStringList{"Key", boundKeys.toString(QKeySequence::PortableText)}, KConfig::Notify);
    buttonGroup.sync();

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    // 0x119 is Qt::ExtraButton7
    Test::pointerButtonPressed(0x119, timestamp++);

    QEXPECT_FAIL("delete", "https://bugreports.qt.io/browse/QTBUG-123135", Abort);
    QEXPECT_FAIL("keypad delete", "https://bugreports.qt.io/browse/QTBUG-123135", Abort);
    QVERIFY(keyChangedSpy.wait());
    QFETCH(QList<quint32>, expectedKeys);
    QCOMPARE(keyChangedSpy.count(), expectedKeys.count());
    for (int i = 0; i < keyChangedSpy.count(); i++) {
        QCOMPARE(keyChangedSpy.at(i).at(0).value<quint32>(), expectedKeys.at(i));
        QCOMPARE(keyChangedSpy.at(i).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    }
    Test::pointerButtonReleased(0x119, timestamp++);
}

WAYLANDTEST_MAIN(TestButtonRebind)
#include "buttonrebind_test.moc"
