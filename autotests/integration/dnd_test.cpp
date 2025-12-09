/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "utils/pipe.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/dataoffer.h>
#include <KWayland/Client/datasource.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QMimeType>
#include <QtConcurrent>

#include <fcntl.h>
#include <linux/input-event-codes.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_dnd-0");

static QFuture<QByteArray> readMimeTypeData(KWayland::Client::DataOffer *offer, const QString &mimeType)
{
    auto pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        return QFuture<QByteArray>();
    }

    offer->receive(mimeType, pipe->writeEndpoint.get());

    return QtConcurrent::run([fd = std::move(pipe->readEndpoint)] {
        QFile file;
        if (!file.open(fd.get(), QFile::ReadOnly | QFile::Text)) {
            return QByteArray();
        }

        return file.readAll();
    });
}

template<typename T>
static bool waitFuture(const QFuture<T> &future)
{
    QFutureWatcher<T> watcher;
    QSignalSpy finishedSpy(&watcher, &QFutureWatcher<T>::finished);
    watcher.setFuture(future);
    return finishedSpy.wait();
}

class DndTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void pointerDrag();
    void pointerSubSurfaceDrag();
    void pointerDragAction_data();
    void pointerDragAction();
    void inertPointerDragOffer();
    void invalidSerialForPointerDrag();
    void cancelPointerDrag();
    void destroyPointerDragSource();
    void noAcceptedMimeTypePointerDrag();
    void noSelectedActionPointerDrag();
    void secondPointerDrag();
    void touchDrag();
    void touchSubSurfaceDrag();
    void touchDragAction_data();
    void touchDragAction();
    void inertTouchDragOffer();
    void invalidSerialForTouchDrag();
    void noAcceptedMimeTypeTouchDrag();
    void noSelectedActionTouchDrag();
    void secondTouchDrag();
    void cancelTouchSequence();
    void tabletDrag();
    void tabletSubSurfaceDrag();
    void tabletDragAction_data();
    void tabletDragAction();
    void inertTabletDragOffer();
    void invalidSerialForTabletDrag();
    void noAcceptedMimeTypeTabletDrag();
    void noSelectedActionTabletDrag();
    void secondTabletDrag();

private:
    QMimeDatabase m_mimeDb;
};

void DndTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));

    // Generate tablet tool events so the corresponding tablet tool device is registered.
    Test::tabletToolProximityEvent(QPointF(640, 480), 0, 0, 0, 0, true, 0, 0);
    Test::tabletToolProximityEvent(QPointF(640, 480), 0, 0, 0, 0, false, 0, 0);
}

void DndTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager | Test::AdditionalWaylandInterface::WpTabletV2));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void DndTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DndTest::pointerDrag()
{
    // This test verifies that a simple pointer drag-and-drop operation between two Wayland clients works as expected.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(dataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("foo");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy pointerEnteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer, &KWayland::Client::Pointer::left);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup the source client.
    auto sourceSurface = Test::createSurface();
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(sourceWindow);
    sourceWindow->move(QPointF(0, 0));

    // Setup the target client.
    auto targetSurface = Test::createSurface();
    auto targetShellSurface = Test::createXdgToplevelSurface(targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(targetWindow);
    targetWindow->move(QPointF(100, 0));

    // Move the pointer to the center of the source window and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    const quint32 buttonPressSerial = pointerButtonSpy.last().at(0).value<quint32>();
    dataDevice->startDrag(buttonPressSerial, dataSource, sourceSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(pointerLeftSpy.count(), 1);
    QVERIFY(!pointer->enteredSurface());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), sourceSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 0);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept our own offer.
    auto sourceOffer = dataDevice->takeDragOffer();
    QCOMPARE(sourceOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(sourceOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    sourceOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    sourceOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy sourceOfferSelectedDragAndDropActionSpy(sourceOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    sourceOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(sourceOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);

    // Move the pointer a little bit.
    Test::pointerMotion(QPointF(60, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Move the pointer to the second client.
    Test::pointerMotion(QPointF(140, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(40, 50));
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), targetSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept the offer.
    auto targetOffer = dataDevice->takeDragOffer();
    QCOMPARE(targetOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(targetOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    targetOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    targetOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(targetOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    targetOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the pointer a little bit.
    Test::pointerMotion(QPointF(160, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(pointer->enteredSurface(), targetSurface.get());
    QCOMPARE(pointerEnteredSpy.last().at(1).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/plain"));
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/html"));
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    targetOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::pointerSubSurfaceDrag()
{
    // This test verifies that a drag-and-drop originating from a sub-surface works as expected.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy pointerEnteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer, &KWayland::Client::Pointer::left);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup a window with a subsurface that covers the right half.
    auto parentSurface = Test::createSurface();
    auto parentShellSurface = Test::createXdgToplevelSurface(parentSurface.get());
    auto window = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    auto childSurface = Test::createSurface();
    auto subSurface = Test::createSubSurface(childSurface.get(), parentSurface.get());
    subSurface->setPosition(QPoint(50, 0));
    Test::render(childSurface.get(), QSize(50, 100), Qt::blue);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Move the pointer to the center of the subsurface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(75, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    const quint32 buttonPressSerial = pointerButtonSpy.last().at(0).value<quint32>();
    dataDevice->startDrag(buttonPressSerial, dataSource, childSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(pointerLeftSpy.count(), 1);
    QVERIFY(!pointer->enteredSurface());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Move the pointer to the parent surface.
    Test::pointerMotion(QPointF(10, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(10, 50));
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    Test::pointerMotion(QPointF(15, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(15, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    // Move the pointer to the child surface.
    Test::pointerMotion(QPointF(80, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(30, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    Test::pointerMotion(QPointF(75, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Accept the data offer.
    auto offer = dataDevice->takeDragOffer();
    QCOMPARE(offer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    offer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(offer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(pointer->enteredSurface(), childSurface.get());
    QCOMPARE(pointerEnteredSpy.last().at(1).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::pointerDragAction_data()
{
    QTest::addColumn<KWayland::Client::DataDeviceManager::DnDAction>("action");

    QTest::addRow("ask") << KWayland::Client::DataDeviceManager::DnDAction::Ask;
    QTest::addRow("copy") << KWayland::Client::DataDeviceManager::DnDAction::Copy;
    QTest::addRow("move") << KWayland::Client::DataDeviceManager::DnDAction::Move;
}

void DndTest::pointerDragAction()
{
    // This test verifies that the dnd actions are handled as expected. "Ask" action is a bit
    // special; if it has been selected, then the final action can be set after the drop.

    QVERIFY(Test::waitForWaylandPointer());
    QFETCH(KWayland::Client::DataDeviceManager::DnDAction, action);

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move | KWayland::Client::DataDeviceManager::DnDAction::Ask);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    offer->setDragAndDropActions(action, action);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);
    QCOMPARE(dataSource->selectedDragAndDropAction(), action);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);

    // Change action after drop.
    switch (action) {
    case KWayland::Client::DataDeviceManager::DnDAction::Ask:
        offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
        QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
        QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 2);
        QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
        break;
    case KWayland::Client::DataDeviceManager::DnDAction::Copy:
    case KWayland::Client::DataDeviceManager::DnDAction::Move: {
        const auto oppositeAction = action == KWayland::Client::DataDeviceManager::DnDAction::Copy
            ? KWayland::Client::DataDeviceManager::DnDAction::Move
            : KWayland::Client::DataDeviceManager::DnDAction::Copy;
        offer->setDragAndDropActions(oppositeAction, oppositeAction);
        QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));
        break;
    }
    case KWayland::Client::DataDeviceManager::DnDAction::None:
        Q_UNREACHABLE();
    }

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::inertPointerDragOffer()
{
    // This test verifies that requests from a previous data offer don't affect the current drag-and-drop operation status.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto firstOffer = dataDevice->takeDragOffer();
    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the pointer outside the surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());

    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Move the pointer back inside the surface, it will result in a new data offer.
    Test::pointerMotion(QPointF(75, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto secondOffer = dataDevice->takeDragOffer();
    secondOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    secondOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    secondOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // The first offer shouldn't change the accepted mime type or selected action.
    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);

    firstOffer->dragAndDropFinished();
    QVERIFY(!dragAndDropFinishedSpy.wait(10));

    secondOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::invalidSerialForPointerDrag()
{
    // This test verifies that the data source will be canceled if a dnd operation is forbidden.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Setup a window with a subsurface that covers the right half.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the subsurface and click the left mouse button.
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(75, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // The dnd operation will be rejected because LMB is already released.
    QSignalSpy dataSourceCanceledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataSourceCanceledSpy.wait());
}

void DndTest::cancelPointerDrag()
{
    // This test verifies that a drag-and-operation will be canceled after pressing the Esc key.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Press the Esc key.
    Test::keyboardKeyPressed(KEY_ESC, timestamp++);
    Test::keyboardKeyReleased(KEY_ESC, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void DndTest::destroyPointerDragSource()
{
    // This test verifies that a drag-and-drop operation will be cancelled when the source is destroyed.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Nuke the data source.
    delete dataSource;
    dataSource = nullptr;
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void DndTest::noAcceptedMimeTypePointerDrag()
{
    // This test verifies that a drag-and-drop operation will be canceled if no mime type has been accepted.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without accepted mime type, the drag should be canceled when the LMB is released.
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);
    auto offer = dataDevice->takeDragOffer();
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());

    // Finish the drag-and-drop operation.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::noSelectedActionPointerDrag()
{
    // This test verifies that a drag-and-drop operation will be canceled if no action has been accepted.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without selected action, the drag should be canceled when the LMB is released.
    QSignalSpy dataSourceTargetAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(dataSourceTargetAcceptsSpy.wait());

    // Finish the drag-and-drop operation.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::secondPointerDrag()
{
    // This test verifies that no second drag operation will be started while there's another active drag.

    QVERIFY(Test::waitForWaylandPointer());

    // Setup the data source.
    auto pointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy pointerButtonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    auto secondDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    secondDataSource->offer(QStringLiteral("text/plain"));
    secondDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Move the pointer to the center of the surface and press the left mouse button.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pointerButtonSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Attempt to start another drag-and-drop operation.
    QSignalSpy secondDataSourceCancelledSpy(secondDataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(pointerButtonSpy.last().at(0).value<quint32>(), secondDataSource, surface.get());
    QVERIFY(secondDataSourceCancelledSpy.wait());
    QCOMPARE(dataSourceCancelledSpy.count(), 0);

    // Finish the drag-and-drop operation.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::touchDrag()
{
    // This test verifies that a simple touch drag-and-drop operation between two Wayland surfaces works as expected.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(dataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("foo");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy touchSequenceEndedSpy(touch, &KWayland::Client::Touch::sequenceEnded);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup the source client.
    auto sourceSurface = Test::createSurface();
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(sourceWindow);
    sourceWindow->move(QPointF(0, 0));

    // Setup the target client.
    auto targetSurface = Test::createSurface();
    auto targetShellSurface = Test::createXdgToplevelSurface(targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(targetWindow);
    targetWindow->move(QPointF(100, 0));

    // Tap the source surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, sourceSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), sourceSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 0);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept our own offer.
    auto sourceOffer = dataDevice->takeDragOffer();
    QCOMPARE(sourceOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(sourceOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    sourceOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    sourceOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy sourceOfferSelectedDragAndDropActionSpy(sourceOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    sourceOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(sourceOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);

    // Move the finger a little bit.
    Test::touchMotion(0, QPointF(60, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // No drag motion event will be sent if an unrelated finger moves.
    Test::touchDown(1, QPointF(40, 40), timestamp++);
    Test::touchMotion(1, QPointF(45, 45), timestamp++);
    Test::touchUp(1, timestamp++);
    QVERIFY(!dataDeviceDragMotionSpy.wait(10));

    // Move the finger to the target surface.
    Test::touchMotion(0, QPointF(140, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(40, 50));
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), targetSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept the offer.
    auto targetOffer = dataDevice->takeDragOffer();
    QCOMPARE(targetOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(targetOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    targetOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    targetOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(targetOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    targetOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the finger a little bit.
    Test::touchMotion(0, QPointF(160, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(touchSequenceEndedSpy.wait());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/plain"));
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/html"));
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    targetOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::touchSubSurfaceDrag()
{
    // This test verifies that a drag-and-drop originating from a sub-surface works as expected.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy touchSequenceEndedSpy(touch, &KWayland::Client::Touch::sequenceEnded);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup a window with a subsurface that covers the right half.
    auto parentSurface = Test::createSurface();
    auto parentShellSurface = Test::createXdgToplevelSurface(parentSurface.get());
    auto window = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    auto childSurface = Test::createSurface();
    auto subSurface = Test::createSubSurface(childSurface.get(), parentSurface.get());
    subSurface->setPosition(QPoint(50, 0));
    Test::render(childSurface.get(), QSize(50, 100), Qt::blue);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Tap the child surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(75, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, childSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Move the finger to the parent surface.
    Test::touchMotion(0, QPointF(10, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(10, 50));
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    Test::touchMotion(0, QPointF(15, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(15, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    // Move the finger to the child surface.
    Test::touchMotion(0, QPointF(80, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(30, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    Test::touchMotion(0, QPointF(75, 50), timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Accept the data offer.
    auto offer = dataDevice->takeDragOffer();
    QCOMPARE(offer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    offer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(offer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(touchSequenceEndedSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::touchDragAction_data()
{
    QTest::addColumn<KWayland::Client::DataDeviceManager::DnDAction>("action");

    QTest::addRow("ask") << KWayland::Client::DataDeviceManager::DnDAction::Ask;
    QTest::addRow("copy") << KWayland::Client::DataDeviceManager::DnDAction::Copy;
    QTest::addRow("move") << KWayland::Client::DataDeviceManager::DnDAction::Move;
}

void DndTest::touchDragAction()
{
    // This test verifies that the dnd actions are handled as expected. "Ask" action is a bit
    // special; if it has been selected, then the final action can be set after the drop.

    QVERIFY(Test::waitForWaylandTouch());
    QFETCH(KWayland::Client::DataDeviceManager::DnDAction, action);

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move | KWayland::Client::DataDeviceManager::DnDAction::Ask);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Touch the surface in the center.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    offer->setDragAndDropActions(action, action);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);
    QCOMPARE(dataSource->selectedDragAndDropAction(), action);

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);

    // Change action after drop.
    switch (action) {
    case KWayland::Client::DataDeviceManager::DnDAction::Ask:
        offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
        QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
        QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 2);
        QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
        break;
    case KWayland::Client::DataDeviceManager::DnDAction::Copy:
    case KWayland::Client::DataDeviceManager::DnDAction::Move: {
        const auto oppositeAction = action == KWayland::Client::DataDeviceManager::DnDAction::Copy
            ? KWayland::Client::DataDeviceManager::DnDAction::Move
            : KWayland::Client::DataDeviceManager::DnDAction::Copy;
        offer->setDragAndDropActions(oppositeAction, oppositeAction);
        QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));
        break;
    }
    case KWayland::Client::DataDeviceManager::DnDAction::None:
        Q_UNREACHABLE();
    }

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::inertTouchDragOffer()
{
    // This test verifies that requests from a previous data offer don't affect the current drag-and-drop operation status.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface in the center.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto firstOffer = dataDevice->takeDragOffer();
    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the finger outside the surface.
    Test::touchMotion(0, QPointF(150, 50), timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());

    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Move the finger back inside the surface, it will result in a new data offer.
    Test::touchMotion(0, QPointF(75, 50), timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto secondOffer = dataDevice->takeDragOffer();
    secondOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    secondOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    secondOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // The first offer shouldn't change the accepted mime type or selected action.

    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);

    firstOffer->dragAndDropFinished();
    QVERIFY(!dragAndDropFinishedSpy.wait(10));

    secondOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::invalidSerialForTouchDrag()
{
    // This test verifies that the data source will be canceled if a dnd operation is forbidden.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Setup a window with a subsurface that covers the right half.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface.
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(75, 50), timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // The dnd operation will be rejected because the touch point is already released.
    QSignalSpy dataSourceCanceledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataSourceCanceledSpy.wait());
}

void DndTest::noAcceptedMimeTypeTouchDrag()
{
    // This test verifies that a drag-and-drop operation will be cancelled if no mime type is accepted.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without accepted mime type, the drag should be canceled when the finger is lifted.
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);
    auto offer = dataDevice->takeDragOffer();
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::noSelectedActionTouchDrag()
{
    // This test verifies that a drag-and-drop operation will be cancelled if no action is accepted.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without selected action, the drag should be canceled when the finger is lifted.
    QSignalSpy dataSourceTargetAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(dataSourceTargetAcceptsSpy.wait());

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::secondTouchDrag()
{
    // This test verifies that no second drag operation will be started while there's another active drag.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    auto secondDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    secondDataSource->offer(QStringLiteral("text/plain"));
    secondDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    const quint32 downSerial = touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial();
    dataDevice->startDrag(downSerial, dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Attempt to start another drag-and-drop operation.
    QSignalSpy secondDataSourceCancelledSpy(secondDataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(downSerial, secondDataSource, surface.get());
    QVERIFY(secondDataSourceCancelledSpy.wait());
    QCOMPARE(dataSourceCancelledSpy.count(), 0);

    // Drop.
    Test::touchUp(0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::cancelTouchSequence()
{
    // This test verifies that a drag-and-drop session will be cancelled when the input backend generates a touch cancel event.

    QVERIFY(Test::waitForWaylandTouch());

    // Setup the data source.
    auto touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy touchSequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Tap the surface.
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QVERIFY(touchSequenceStartedSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(touchSequenceStartedSpy.last().at(0).value<KWayland::Client::TouchPoint *>()->downSerial(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Cancel the touch sequence.
    Test::touchCancel();
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 0);
}

void DndTest::tabletDrag()
{
    // This test verifies that a simple tablet drag-and-drop operation between two Wayland surfaces works as expected.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(dataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("foo");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup the source client.
    auto sourceSurface = Test::createSurface();
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(sourceWindow);
    sourceWindow->move(QPointF(0, 0));

    // Setup the target client.
    auto targetSurface = Test::createSurface();
    auto targetShellSurface = Test::createXdgToplevelSurface(targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(targetWindow);
    targetWindow->move(QPointF(100, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, sourceSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), sourceSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 0);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept our own offer.
    auto sourceOffer = dataDevice->takeDragOffer();
    QCOMPARE(sourceOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(sourceOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    sourceOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    sourceOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy sourceOfferSelectedDragAndDropActionSpy(sourceOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    sourceOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(sourceOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(sourceOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);

    // Move the tablet tool a little bit.
    Test::tabletToolAxisEvent(QPointF(60, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 1);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 0);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Move the tablet tool to the target surface.
    Test::tabletToolAxisEvent(QPointF(140, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(40, 50));
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), targetSurface.get());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 1);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Accept the offer.
    auto targetOffer = dataDevice->takeDragOffer();
    QCOMPARE(targetOffer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(targetOffer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    targetOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    targetOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(targetOffer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    targetOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(targetOffer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the tablet tool a little bit.
    Test::tabletToolAxisEvent(QPointF(160, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(60, 50));

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);

    // Drop.
    Test::tabletToolTipEvent(QPointF(160, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(160, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());

    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragMotionSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/plain"));
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(targetOffer.get(), QStringLiteral("text/html"));
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    targetOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::tabletSubSurfaceDrag()
{
    // This test verifies that a drag-and-drop originating from a sub-surface works as expected.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);

    // Setup a window with a subsurface that covers the right half.
    auto parentSurface = Test::createSurface();
    auto parentShellSurface = Test::createXdgToplevelSurface(parentSurface.get());
    auto window = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    auto childSurface = Test::createSurface();
    auto subSurface = Test::createSubSurface(childSurface.get(), parentSurface.get());
    subSurface->setPosition(QPoint(50, 0));
    Test::render(childSurface.get(), QSize(50, 100), Qt::blue);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Press the tip of the tablet tool in the center of the child surface.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(75, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(75, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, childSurface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Move the tablet tool to the parent surface.
    Test::tabletToolAxisEvent(QPointF(10, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(10, 50));
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    Test::tabletToolAxisEvent(QPointF(15, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(15, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 2);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 1);
    QCOMPARE(dataDevice->dragSurface(), parentSurface.get());

    // Move the tablet tool to the child surface.
    Test::tabletToolAxisEvent(QPointF(80, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(30, 50));
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    Test::tabletToolAxisEvent(QPointF(75, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragMotionSpy.wait());
    QCOMPARE(dataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(dataDeviceDragEnteredSpy.count(), 3);
    QCOMPARE(dataDeviceDragLeftSpy.count(), 2);
    QCOMPARE(dataDevice->dragSurface(), childSurface.get());

    // Accept the data offer.
    auto offer = dataDevice->takeDragOffer();
    QCOMPARE(offer->offeredMimeTypes(), (QList<QMimeType>{m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"))}));
    QCOMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);

    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    offer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    QSignalSpy targetOfferSelectedDragAndDropActionSpy(offer.get(), &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(targetOfferSelectedDragAndDropActionSpy.wait());
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Drop.
    Test::tabletToolTipEvent(QPointF(75, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(75, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 1);

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::tabletDragAction_data()
{
    QTest::addColumn<KWayland::Client::DataDeviceManager::DnDAction>("action");

    QTest::addRow("ask") << KWayland::Client::DataDeviceManager::DnDAction::Ask;
    QTest::addRow("copy") << KWayland::Client::DataDeviceManager::DnDAction::Copy;
    QTest::addRow("move") << KWayland::Client::DataDeviceManager::DnDAction::Move;
}

void DndTest::tabletDragAction()
{
    // This test verifies that the dnd actions are handled as expected. "Ask" action is a bit
    // special; if it has been selected, then the final action can be set after the drop.

    QFETCH(KWayland::Client::DataDeviceManager::DnDAction, action);

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move | KWayland::Client::DataDeviceManager::DnDAction::Ask);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    offer->setDragAndDropActions(action, action);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);
    QCOMPARE(dataSource->selectedDragAndDropAction(), action);

    // Drop.
    Test::tabletToolTipEvent(QPointF(50, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());
    QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 1);

    // Change action after drop.
    switch (action) {
    case KWayland::Client::DataDeviceManager::DnDAction::Ask:
        offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
        QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
        QCOMPARE(dataSourceSelectedDragAndDropActionChangedSpy.count(), 2);
        QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
        break;
    case KWayland::Client::DataDeviceManager::DnDAction::Copy:
    case KWayland::Client::DataDeviceManager::DnDAction::Move: {
        const auto oppositeAction = action == KWayland::Client::DataDeviceManager::DnDAction::Copy
            ? KWayland::Client::DataDeviceManager::DnDAction::Move
            : KWayland::Client::DataDeviceManager::DnDAction::Copy;
        offer->setDragAndDropActions(oppositeAction, oppositeAction);
        QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));
        break;
    }
    case KWayland::Client::DataDeviceManager::DnDAction::None:
        Q_UNREACHABLE();
    }

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::inertTabletDragOffer()
{
    // This test verifies that requests from a previous data offer don't affect the current drag-and-drop operation status.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy sourceTargetsAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    // Setup the source window.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto firstOffer = dataDevice->takeDragOffer();
    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Move the tablet tool tip outside the surface.
    Test::tabletToolAxisEvent(QPointF(150, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());

    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Move the finger back inside the surface, it will result in a new data offer.
    Test::tabletToolAxisEvent(QPointF(75, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(dataDeviceDragEnteredSpy.wait());

    const auto secondOffer = dataDevice->takeDragOffer();
    secondOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QString());

    secondOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(sourceTargetsAcceptsSpy.wait());
    QCOMPARE(sourceTargetsAcceptsSpy.last().at(0).value<QString>(), QStringLiteral("text/plain"));

    secondOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());
    QCOMPARE(dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // The first offer shouldn't change the accepted mime type or selected action.
    firstOffer->accept(QString(), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(!sourceTargetsAcceptsSpy.wait(10));

    firstOffer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(!dataSourceSelectedDragAndDropActionChangedSpy.wait(10));

    // Drop.
    Test::tabletToolTipEvent(QPointF(75, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(75, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDroppedSpy.wait());

    // Finish drag-and-drop.
    QSignalSpy dragAndDropFinishedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropFinished);

    firstOffer->dragAndDropFinished();
    QVERIFY(!dragAndDropFinishedSpy.wait(10));

    secondOffer->dragAndDropFinished();
    QVERIFY(dragAndDropFinishedSpy.wait());
}

void DndTest::invalidSerialForTabletDrag()
{
    // This test verifies that the data source will be canceled if a dnd operation is forbidden.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Setup a window with a subsurface that covers the right half.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // The dnd operation will be rejected because the touch point is already released.
    QSignalSpy dataSourceCanceledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, surface.get());
    QVERIFY(dataSourceCanceledSpy.wait());
}

void DndTest::noAcceptedMimeTypeTabletDrag()
{
    // This test verifies that a drag-and-drop operation will be cancelled if no mime type is accepted.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without accepted mime type, the drag should be canceled when the finger is lifted.
    QSignalSpy dataSourceSelectedDragAndDropActionChangedSpy(dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);
    auto offer = dataDevice->takeDragOffer();
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(dataSourceSelectedDragAndDropActionChangedSpy.wait());

    // Drop.
    Test::tabletToolTipEvent(QPointF(50, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::noSelectedActionTabletDrag()
{
    // This test verifies that a drag-and-drop operation will be cancelled if no action is accepted.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    dataDevice->startDrag(tabletToolDownSpy.last().at(0).value<uint32_t>(), dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Without selected action, the drag should be canceled when the finger is lifted.
    QSignalSpy dataSourceTargetAcceptsSpy(dataSource, &KWayland::Client::DataSource::targetAccepts);
    auto offer = dataDevice->takeDragOffer();
    offer->accept(QStringLiteral("text/plain"), dataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    QVERIFY(dataSourceTargetAcceptsSpy.wait());

    // Drop.
    Test::tabletToolTipEvent(QPointF(50, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

void DndTest::secondTabletDrag()
{
    // This test verifies that no second drag operation will be started while there's another active drag.

    std::unique_ptr<Test::WpTabletSeatV2> tabletSeat = Test::tabletManager()->createSeat(Test::waylandSeat());
    QSignalSpy toolAddedSpy(tabletSeat.get(), &Test::WpTabletSeatV2::toolAdded);
    QVERIFY(toolAddedSpy.wait());
    Test::WpTabletToolV2 *tabletTool = toolAddedSpy.last().at(0).value<Test::WpTabletToolV2 *>();
    QVERIFY(Test::waitForWaylandTabletTool(tabletTool));

    // Setup the data source.
    auto dataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto dataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    dataSource->offer(QStringLiteral("text/plain"));
    dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    QSignalSpy dataDeviceDragEnteredSpy(dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dataDeviceDragLeftSpy(dataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy dataDeviceDragMotionSpy(dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy dataDeviceDroppedSpy(dataDevice, &KWayland::Client::DataDevice::dropped);
    QSignalSpy dataSourceCancelledSpy(dataSource, &KWayland::Client::DataSource::cancelled);
    QSignalSpy dataSourceDropPerformedSpy(dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    auto secondDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    secondDataSource->offer(QStringLiteral("text/plain"));
    secondDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);

    // Create a surface.
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(window);
    window->move(QPointF(0, 0));

    // Press the tip of the tablet tool in the center of the window.
    QSignalSpy tabletToolDownSpy(tabletTool, &Test::WpTabletToolV2::down);
    quint32 timestamp = 0;
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, true, 0, timestamp++);
    Test::tabletToolTipEvent(QPointF(50, 50), 1, 0, 0, 0, 0, true, 0, timestamp++);
    QVERIFY(tabletToolDownSpy.wait());

    // Start a drag-and-drop operation.
    const quint32 downSerial = tabletToolDownSpy.last().at(0).value<uint32_t>();
    dataDevice->startDrag(downSerial, dataSource, surface.get());
    QVERIFY(dataDeviceDragEnteredSpy.wait());
    QCOMPARE(dataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(dataDevice->dragSurface(), surface.get());

    // Attempt to start another drag-and-drop operation.
    QSignalSpy secondDataSourceCancelledSpy(secondDataSource, &KWayland::Client::DataSource::cancelled);
    dataDevice->startDrag(downSerial, secondDataSource, surface.get());
    QVERIFY(secondDataSourceCancelledSpy.wait());
    QCOMPARE(dataSourceCancelledSpy.count(), 0);

    // Drop.
    Test::tabletToolTipEvent(QPointF(50, 50), 0, 0, 0, 0, 0, false, 0, timestamp++);
    Test::tabletToolProximityEvent(QPointF(50, 50), 0, 0, 0, 0, false, 0, timestamp++);
    QVERIFY(dataDeviceDragLeftSpy.wait());
    QCOMPARE(dataDeviceDroppedSpy.count(), 0);
    QCOMPARE(dataSourceCancelledSpy.count(), 1);
    QCOMPARE(dataSourceDropPerformedSpy.count(), 1);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::DndTest)
#include "dnd_test.moc"
