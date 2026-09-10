/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/subsurface.h>

namespace KWin
{

class TabletInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testBasics();
    void testImplicitGrab();
    void testImplicitGrabOnSubsurface();

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
};

void TabletInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(qAppName()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    qputenv("XCURSOR_THEME", QByteArrayLiteral("breeze_cursors"));
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
        Rect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), Rect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), Rect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void TabletInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpTabletV2));
    QVERIFY(Test::waitForWaylandPointer());
    m_compositor = Test::waylandCompositor();
    m_seat = Test::waylandSeat();

    workspace()->setActiveOutput(QPoint(640, 512));

    // Generate tablet tool events so the corresponding tablet tool device is registered.
    Test::tabletToolProximityEvent(QPointF(640, 480), 0, 0, 0, 0, true, 0, 0);
    Test::tabletToolProximityEvent(QPointF(640, 480), 0, 0, 0, 0, false, 0, 0);
}

void TabletInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TabletInputTest::testBasics()
{
    // tests basic input of tablet tools

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    window.m_window->move(QPointF(0, 0));

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::kwinSeat());

    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    uint32_t time = 0;
    QSignalSpy frame(tabletTool, &Test::WpTabletToolV2::frame);
    QSignalSpy motion(tabletTool, &Test::WpTabletToolV2::motion);
    QSignalSpy down(tabletTool, &Test::WpTabletToolV2::down);
    QSignalSpy up(tabletTool, &Test::WpTabletToolV2::up);
    QSignalSpy proximityIn(tabletTool, &Test::WpTabletToolV2::proximityIn);
    QSignalSpy proximityOut(tabletTool, &Test::WpTabletToolV2::proximityOut);
    QSignalSpy button(tabletTool, &Test::WpTabletToolV2::button);

    // proximity enter
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 0);
    QCOMPARE(up.count(), 0);
    QCOMPARE(motion.count(), 1);
    QCOMPARE(motion.last().at(0).value<QPointF>(), QPointF(50, 50));

    // motion in proximity
    Test::tabletToolProximityEvent(QPointF(51, 49), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 0);
    QCOMPARE(up.count(), 0);
    QCOMPARE(motion.count(), 2);
    QCOMPARE(motion.last().at(0).value<QPointF>(), QPointF(51, 49));

    // button press in proximity
    Test::tabletToolButtonPressed(1, time++);
    QVERIFY(button.wait());
    QCOMPARE(button.count(), 1);
    QCOMPARE(button.last().at(1).value<uint32_t>(), 1);
    QCOMPARE(button.last().at(2).value<uint32_t>(), 1);

    Test::tabletToolButtonReleased(1, time++);
    QVERIFY(button.wait());
    QCOMPARE(button.count(), 2);
    QCOMPARE(button.last().at(1).value<uint32_t>(), 1);
    QCOMPARE(button.last().at(2).value<uint32_t>(), 0);

    // tip without a button
    Test::tabletToolTipEvent(QPointF(50, 50), 1, -0.5, 0.5, 0.5, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);
    QCOMPARE(motion.count(), 3);
    QCOMPARE(motion.last().at(0).value<QPointF>(), QPointF(50, 50));

    // motion with the tip down
    Test::tabletToolTipEvent(QPointF(49, 51), 1, -0.5, 0.5, 0.5, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 0);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);
    QCOMPARE(motion.count(), 4);
    QCOMPARE(motion.last().at(0).value<QPointF>(), QPointF(49, 51));

    // button press with the tip down
    Test::tabletToolButtonPressed(1, time++);
    QVERIFY(button.wait());
    QCOMPARE(button.count(), 3);
    QCOMPARE(button.last().at(1).value<uint32_t>(), 1);
    QCOMPARE(button.last().at(2).value<uint32_t>(), 1);

    Test::tabletToolButtonReleased(1, time++);
    QVERIFY(button.wait());
    QCOMPARE(button.count(), 4);
    QCOMPARE(button.last().at(1).value<uint32_t>(), 1);
    QCOMPARE(button.last().at(2).value<uint32_t>(), 0);

    // tip lifted
    Test::tabletToolTipEvent(QPointF(50, 50), 1, -0.5, 0.5, 0.5, 0, false, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 1);

    // proximity leave
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 1);
    QCOMPARE(up.count(), 1);

    // proximity enter with a button pressed
    Test::tabletToolButtonPressed(1, time++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(button.count(), 5);
    QCOMPARE(button.last().at(1).value<uint32_t>(), 1);
    QCOMPARE(button.last().at(2).value<uint32_t>(), 1);

    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, time++);
    QVERIFY(frame.wait());

    Test::tabletToolButtonReleased(1, time++);
}

void TabletInputTest::testImplicitGrab()
{
    Test::XdgToplevelWindow window;
    QVERIFY(window.show(QSize(100, 100)));
    window.m_window->move(QPointF(0, 0));

    // This test verifies that implicit grabs works as expected on toplevel surfaces
    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::kwinSeat());

    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    uint32_t time = 0;
    QSignalSpy frame(tabletTool, &Test::WpTabletToolV2::frame);
    QSignalSpy proximityIn(tabletTool, &Test::WpTabletToolV2::proximityIn);
    QSignalSpy proximityOut(tabletTool, &Test::WpTabletToolV2::proximityOut);
    QSignalSpy down(tabletTool, &Test::WpTabletToolV2::down);
    QSignalSpy up(tabletTool, &Test::WpTabletToolV2::up);

    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(tabletTool->enteredSurface(), *window.m_surface);

    // while a button is pressed, moving the tool out of the
    // surface should keep it focused
    Test::tabletToolButtonPressed(1, time++);
    Test::tabletToolProximityEvent(QPointF(150, 150), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 0);

    // releasing the button should trigger the proximity out event
    Test::tabletToolButtonReleased(1, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(proximityOut.count(), 1);

    // same for when the tip is down
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 2);

    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 2);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);

    Test::tabletToolTipEvent(QPointF(150, 150), 1, 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 1);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);

    Test::tabletToolTipEvent(QPointF(150, 150), 1, 0, 0, 0, 0, false, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 2);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 1);

    Test::tabletToolProximityEvent(QPointF(150, 150), 0, 0, 0, 0, false, 0, time++);
}

void TabletInputTest::testImplicitGrabOnSubsurface()
{
    // This test verifies that implicit grabs also work as expected between subsurfaces
    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::kwinSeat());

    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    Test::XdgToplevelWindow window;
    QVERIFY(window.show(QSize(100, 100)));
    window.m_window->move(QPointF(0, 0));

    auto surface = Test::createSurface();
    auto subsurface = Test::createSubSurface(surface.get(), window.m_surface.get());
    Test::render(surface.get(), QSize(50, 50), Qt::blue);
    QVERIFY(window.presentWait());

    uint32_t time = 0;
    QSignalSpy frame(tabletTool, &Test::WpTabletToolV2::frame);
    QSignalSpy proximityIn(tabletTool, &Test::WpTabletToolV2::proximityIn);
    QSignalSpy proximityOut(tabletTool, &Test::WpTabletToolV2::proximityOut);
    QSignalSpy down(tabletTool, &Test::WpTabletToolV2::down);
    QSignalSpy up(tabletTool, &Test::WpTabletToolV2::up);

    Test::tabletToolProximityEvent(QPointF(25, 25), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 1);
    QCOMPARE(tabletTool->enteredSurface(), *surface);

    // while a button is pressed, moving the tool out of the
    // subsurface should keep it focused
    Test::tabletToolButtonPressed(1, time++);
    Test::tabletToolProximityEvent(QPointF(75, 75), 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 0);

    // releasing the button should trigger the leave event
    Test::tabletToolButtonReleased(1, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 1);
    QCOMPARE(proximityIn.count(), 2);
    QCOMPARE(tabletTool->enteredSurface(), *window.m_surface);

    // same for when the tip is down
    Test::tabletToolProximityEvent(QPointF(25, 25), 0, 0, 0, 0, true, 0, time++);
    Test::tabletToolTipEvent(QPointF(25, 25), 1, 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityIn.count(), 3);
    QCOMPARE(proximityOut.count(), 2);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);
    QCOMPARE(tabletTool->enteredSurface(), *surface);

    Test::tabletToolTipEvent(QPointF(75, 75), 1, 0, 0, 0, 0, true, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 2);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 0);

    Test::tabletToolTipEvent(QPointF(75, 75), 1, 0, 0, 0, 0, false, 0, time++);
    QVERIFY(frame.wait());
    QCOMPARE(proximityOut.count(), 3);
    QCOMPARE(down.count(), 1);
    QCOMPARE(up.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::TabletInputTest)
#include "tablet_input_test.moc"
