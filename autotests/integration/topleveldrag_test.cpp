/*
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kwin_wayland_test.h"
#include "wayland/seat.h"
#include "wayland/xdgshell.h"
#include "wayland/xdgtopleveldrag_v1.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/datasource.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_toplevel_drag-0");

class TestToplevelDrag : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testDragging();
    void testAttachUnmappedToplevel();
    void testMapAttachedAfterDragEnd();
};

void TestToplevelDrag::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager | Test::AdditionalWaylandInterface::XdgToplevelDragV1));
}

void TestToplevelDrag::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestToplevelDrag::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
}

void TestToplevelDrag::testDragging()
{
    uint32_t timestamp = 0;

    QVERIFY(Test::waitForWaylandPointer());
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), {100, 100}, Qt::cyan);
    window->move({0, 0});

    Test::pointerMotion({50, 10}, ++timestamp);
    Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonStateChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);
    QVERIFY(enteredSpy.wait());
    QVERIFY(!buttonStateChangedSpy.isEmpty() || buttonStateChangedSpy.wait());
    QCOMPARE(pointer->enteredSurface(), surface.get());
    const QPointF enteredPosition = enteredSpy[0][1].toPointF();
    const uint serial = buttonStateChangedSpy[0][0].toUInt();

    std::unique_ptr<KWayland::Client::DataSource> dataSource(Test::waylandDataDeviceManager()->createDataSource());
    std::unique_ptr<Test::XdgToplevelDragV1> toplevelDrag(Test::toplevelDragManager()->createDrag(dataSource.get()));
    toplevelDrag->attach(shellSurface->object(), enteredPosition.x(), enteredPosition.y());
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat()));
    dataDevice->startDrag(serial, dataSource.get(), surface.get());
    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.wait());
    QVERIFY(waylandServer()->seat()->dragSource());
    QVERIFY(waylandServer()->seat()->xdgTopleveldrag());
    QCOMPARE(waylandServer()->seat()->xdgTopleveldrag()->toplevel()->surface(), window->surface());
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(workspace()->moveResizeWindow(), window);

    Test::pointerMotion({100, 100}, ++timestamp);
    QCOMPARE(window->pos(), QPointF(50, 90));
    Test::pointerButtonReleased(BTN_LEFT, ++timestamp);

    QVERIFY(!window->isInteractiveMove());
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->pos(), QPointF(50, 90));

    Test::pointerMotion({200, 200}, ++timestamp);
    QCOMPARE(window->pos(), QPointF(50, 90));
}

void TestToplevelDrag::testAttachUnmappedToplevel()
{
    uint32_t timestamp = 0;

    QVERIFY(Test::waitForWaylandPointer());
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), {100, 100}, Qt::cyan);
    window->move({0, 0});

    Test::pointerMotion({50, 10}, ++timestamp);
    Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
    QSignalSpy buttonStateChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);
    QVERIFY(!buttonStateChangedSpy.isEmpty() || buttonStateChangedSpy.wait());
    QCOMPARE(pointer->enteredSurface(), surface.get());
    const uint serial = buttonStateChangedSpy[0][0].toUInt();

    std::unique_ptr<KWayland::Client::DataSource> dataSource(Test::waylandDataDeviceManager()->createDataSource());
    std::unique_ptr<Test::XdgToplevelDragV1> toplevelDrag(Test::toplevelDragManager()->createDrag(dataSource.get()));
    auto draggedSurface = Test::createSurface();
    auto draggedToplevel = Test::createXdgToplevelSurface(draggedSurface.get());
    toplevelDrag->attach(draggedToplevel->object(), 0, 0);
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat()));
    dataDevice->startDrag(serial, dataSource.get(), surface.get());
    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.wait());
    QVERIFY(waylandServer()->seat()->dragSource());
    QVERIFY(waylandServer()->seat()->xdgTopleveldrag());

    Test::pointerMotion({100, 100}, ++timestamp);
    QCOMPARE(window->pos(), QPointF(0, 0));

    auto draggedWindow = Test::renderAndWaitForShown(draggedSurface.get(), {100, 100}, Qt::magenta);
    QCOMPARE(waylandServer()->seat()->xdgTopleveldrag()->toplevel()->surface(), draggedWindow->surface());
    QCOMPARE(draggedWindow->pos(), QPointF(100, 100));
    QVERIFY(draggedWindow->isInteractiveMove());
    QCOMPARE(workspace()->moveResizeWindow(), draggedWindow);

    Test::pointerMotion({200, 200}, ++timestamp);
    QCOMPARE(draggedWindow->pos(), QPointF(200, 200));

    Test::pointerButtonReleased(BTN_LEFT, ++timestamp);

    QVERIFY(!draggedWindow->isInteractiveMove());
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(draggedWindow->pos(), QPointF(200, 200));

    Test::pointerMotion({300, 300}, ++timestamp);
    QCOMPARE(draggedWindow->pos(), QPointF(200, 200));
}

void TestToplevelDrag::testMapAttachedAfterDragEnd()
{
    uint32_t timestamp = 0;
    options->setPlacement(PlacementZeroCornered);

    QVERIFY(Test::waitForWaylandPointer());
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), {100, 100}, Qt::cyan);
    window->move({0, 0});

    Test::pointerMotion({50, 10}, ++timestamp);
    Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
    QSignalSpy buttonStateChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);
    QVERIFY(buttonStateChangedSpy.wait());
    QCOMPARE(pointer->enteredSurface(), surface.get());
    const uint serial = buttonStateChangedSpy[0][0].toUInt();

    std::unique_ptr<KWayland::Client::DataSource> dataSource(Test::waylandDataDeviceManager()->createDataSource());
    std::unique_ptr<Test::XdgToplevelDragV1> toplevelDrag(Test::toplevelDragManager()->createDrag(dataSource.get()));
    auto draggedSurface = Test::createSurface();
    auto draggedToplevel = Test::createXdgToplevelSurface(draggedSurface.get());
    toplevelDrag->attach(draggedToplevel->object(), 0, 0);
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat()));
    dataDevice->startDrag(serial, dataSource.get(), surface.get());
    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.wait());
    QVERIFY(waylandServer()->seat()->dragSource());
    QVERIFY(waylandServer()->seat()->xdgTopleveldrag());

    Test::pointerMotion({100, 100}, ++timestamp);
    QCOMPARE(window->pos(), QPointF(0, 0));

    Test::pointerButtonReleased(BTN_LEFT, ++timestamp);

    auto draggedWindow = Test::renderAndWaitForShown(draggedSurface.get(), {100, 100}, Qt::magenta);
    // Center placement policy
    QCOMPARE(draggedWindow->pos(), QPointF(0, 0) + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, draggedWindow)));
    QVERIFY(!draggedWindow->isInteractiveMove());
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);

    Test::pointerMotion({200, 200}, ++timestamp);
    QCOMPARE(draggedWindow->pos(), QPointF(0, 0) + workspace()->cascadeOffset(workspace()->clientArea(PlacementArea, draggedWindow)));
}

WAYLANDTEST_MAIN(TestToplevelDrag)

#include "topleveldrag_test.moc"
