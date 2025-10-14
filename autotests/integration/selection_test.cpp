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
#include <KWayland/Client/surface.h>

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

static const QString s_socketName = QStringLiteral("wayland_test_selection-0");

template<typename Offer>
static QFuture<QByteArray> readMimeTypeData(const Offer &offer, const QMimeType &mimeType)
{
    auto pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        return QFuture<QByteArray>();
    }

    offer->receive(mimeType.name(), pipe->fds[1].get());

    return QtConcurrent::run([fd = std::move(pipe->fds[0])] {
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

static QMimeType plainText()
{
    static QMimeType mimeType = QMimeDatabase().mimeTypeForName(QStringLiteral("text/plain"));
    return mimeType;
}

static QMimeType htmlText()
{
    static QMimeType mimeType = QMimeDatabase().mimeTypeForName(QStringLiteral("text/html"));
    return mimeType;
}

class SelectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void selection();
    void internalSelection();
    void destroySelection();
    void invalidSerialForSelection();
    void unsetSupersededSelection();
    void receiveFromWithdrawnSelectionOffer();
    void singleSelectionPerClient();

    void primarySelection();
    void internalPrimarySelection();
    void destroyPrimarySelection();
    void invalidSerialForPrimarySelection();
    void unsetSupersededPrimarySelection();
    void receiveFromWithdrawnPrimarySelectionOffer();
    void singlePrimarySelectionPerClient();
};

void SelectionTest::initTestCase()
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
}

void SelectionTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void SelectionTest::selection()
{
    // This test verifies that the clipboard works as expected between two clients.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);

    // Setup the source side.
    std::unique_ptr<KWayland::Client::DataDevice> sourceDataDevice(sourceConnection->dataDeviceManager->getDataDevice(sourceConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> dataSource(sourceConnection->dataDeviceManager->createDataSource());
    dataSource->offer(plainText());
    connect(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<KWayland::Client::DataDevice> targetDataDevice(targetConnection->dataDeviceManager->getDataDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the source window.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    sourceDataDevice->setSelection(sourceEnteredSerial, dataSource.get());

    // The target device shouldn't receive an offer while it's not focused.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QVERIFY(!targetDataDeviceSelectionOfferedSpy.wait(100));

    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = targetDataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(offer, htmlText());
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Clear selection.
    QSignalSpy targetDataDeviceSelectionClearedSpy(targetDataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);
    sourceDataDevice->setSelection(sourceEnteredSerial, nullptr);
    QVERIFY(targetDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::internalSelection()
{
    // This test verifies that the source client also receives a wl_data_offer for its own selection.

    auto connection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);

    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(connection->dataDeviceManager->getDataDevice(connection->seat));
    std::unique_ptr<KWayland::Client::DataSource> dataSource(connection->dataDeviceManager->createDataSource());
    dataSource->offer(plainText());
    connect(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto surface = Test::createSurface(connection->compositor);
    auto shellSurface = Test::createXdgToplevelSurface(connection->xdgShell, surface.get());
    auto window = Test::renderAndWaitForShown(connection->shm, surface.get(), QSize(100, 100), Qt::red);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(connection->seat->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(window);
    QVERIFY(keyboardEnteredSpy.wait());
    const quint32 enteredSerial = keyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    dataDevice->setSelection(enteredSerial, dataSource.get());

    QSignalSpy dataDeviceSelectionOfferedSpy(dataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QVERIFY(dataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = dataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(offer, htmlText());
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Clear selection.
    QSignalSpy selectionClearedSpy(dataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);
    dataDevice->setSelection(enteredSerial, nullptr);
    QVERIFY(selectionClearedSpy.wait());
}

void SelectionTest::destroySelection()
{
    // This test verifies that the wl_data_offer will be withdrawn if the associated data source is destroyed.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);

    // Setup the source side.
    std::unique_ptr<KWayland::Client::DataDevice> sourceDataDevice(sourceConnection->dataDeviceManager->getDataDevice(sourceConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> dataSource(sourceConnection->dataDeviceManager->createDataSource());
    dataSource->offer(plainText());
    connect(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<KWayland::Client::DataDevice> targetDataDevice(targetConnection->dataDeviceManager->getDataDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the source window.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    sourceDataDevice->setSelection(sourceEnteredSerial, dataSource.get());

    // Activate the target window and wait for the data offer.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = targetDataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), QList<QMimeType>{plainText()});

    // Destroy the data source.
    QSignalSpy targetDataDeviceSelectionClearedSpy(targetDataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);
    dataSource.reset();
    QVERIFY(targetDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::invalidSerialForSelection()
{
    // This test verifies that a set_selection request will be ignored if the specified serial is invalid.

    auto connection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);

    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(connection->dataDeviceManager->getDataDevice(connection->seat));
    std::unique_ptr<KWayland::Client::DataSource> dataSource(connection->dataDeviceManager->createDataSource());
    dataSource->offer(QStringLiteral("text/plain"));
    connect(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto surface = Test::createSurface(connection->compositor);
    auto shellSurface = Test::createXdgToplevelSurface(connection->xdgShell, surface.get());
    Test::renderAndWaitForShown(connection->shm, surface.get(), QSize(100, 100), Qt::red);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(connection->seat->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.wait());
    const quint32 enteredSerial = keyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    QSignalSpy dataSourceCancelledSpy(dataSource.get(), &KWayland::Client::DataSource::cancelled);
    dataDevice->setSelection(enteredSerial - 100, dataSource.get());
    QVERIFY(dataSourceCancelledSpy.wait(100));
}

void SelectionTest::unsetSupersededSelection()
{
    // This test verifies that the current selection won't be unset if the previous selection is unset with set_selection(null).

    // Setup the first client.
    auto firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    QVERIFY(Test::waitForWaylandKeyboard(firstConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> firstKeyboard(firstConnection->seat->createKeyboard());
    std::unique_ptr<KWayland::Client::DataDevice> firstDataDevice(firstConnection->dataDeviceManager->getDataDevice(firstConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> firstDataSource(firstConnection->dataDeviceManager->createDataSource());
    firstDataSource->offer(plainText());
    connect(firstDataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto firstSurface = Test::createSurface(firstConnection->compositor);
    auto firstShellSurface = Test::createXdgToplevelSurface(firstConnection->xdgShell, firstSurface.get());
    auto firstWindow = Test::renderAndWaitForShown(firstConnection->shm, firstSurface.get(), QSize(100, 100), Qt::red);

    // Setup the second client.
    auto secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    QVERIFY(Test::waitForWaylandKeyboard(secondConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> secondKeyboard(secondConnection->seat->createKeyboard());
    std::unique_ptr<KWayland::Client::DataDevice> secondDataDevice(secondConnection->dataDeviceManager->getDataDevice(secondConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> secondDataSource(secondConnection->dataDeviceManager->createDataSource());
    secondDataSource->offer(htmlText());
    connect(secondDataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("bar");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto secondSurface = Test::createSurface(secondConnection->compositor);
    auto secondShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, secondSurface.get());
    auto secondWindow = Test::renderAndWaitForShown(secondConnection->shm, secondSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the first window and set the selection.
    QSignalSpy firstKeyboardEnteredSpy(firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstDataDeviceSelectionOfferedSpy(firstDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstKeyboardEnteredSpy.wait());

    const quint32 firstEnteredSerial = firstKeyboardEnteredSpy.last().at(0).value<quint32>();
    firstDataDevice->setSelection(firstEnteredSerial, firstDataSource.get());
    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    // Activate the second window and set the selection.
    QSignalSpy secondKeyboardEnteredSpy(secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondDataDeviceSelectionOfferedSpy(secondDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(secondWindow);
    QVERIFY(secondKeyboardEnteredSpy.wait());

    const quint32 secondEnteredSerial = secondKeyboardEnteredSpy.last().at(0).value<quint32>();
    secondDataDevice->setSelection(secondEnteredSerial, secondDataSource.get());
    QVERIFY(secondDataDeviceSelectionOfferedSpy.wait());

    // Attempt to unset the first selection.
    QSignalSpy secondDataDeviceSelectionClearedSpy(secondDataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);
    firstDataDevice->setSelection(firstEnteredSerial, nullptr);
    QVERIFY(!secondDataDeviceSelectionClearedSpy.wait(100));

    // Attempt to unset the second selection.
    secondDataDevice->setSelection(secondEnteredSerial, nullptr);
    QVERIFY(secondDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::receiveFromWithdrawnSelectionOffer()
{
    // This test verifies that a client cannot receive data from a withdrawn selection offer.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);

    // Setup the source side.
    std::unique_ptr<KWayland::Client::DataDevice> sourceDataDevice(sourceConnection->dataDeviceManager->getDataDevice(sourceConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> dataSource(sourceConnection->dataDeviceManager->createDataSource());
    dataSource->offer(plainText());
    connect(dataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<KWayland::Client::DataDevice> targetDataDevice(targetConnection->dataDeviceManager->getDataDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Focus the source window, and set the selection.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();
    sourceDataDevice->setSelection(sourceEnteredSerial, dataSource.get());

    // The target client should be offered the selection when its window gets focused.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    std::unique_ptr<KWayland::Client::DataOffer> offer = targetDataDevice->takeOfferedSelection();
    QCOMPARE(offer->offeredMimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // Remove focus from the target window, it should not be able to receive data anymore.
    workspace()->activateWindow(nullptr);

    const QFuture<QByteArray> emptyData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(emptyData));
    QCOMPARE(emptyData.result(), QByteArray());
}

void SelectionTest::singleSelectionPerClient()
{
    // This test verifies that only one selection event will be sent when switching between two
    // surfaces that belong to the same client.

    // Setup the first client.
    auto firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    QVERIFY(Test::waitForWaylandKeyboard(firstConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> firstKeyboard(firstConnection->seat->createKeyboard());
    std::unique_ptr<KWayland::Client::DataDevice> firstDataDevice(firstConnection->dataDeviceManager->getDataDevice(firstConnection->seat));
    std::unique_ptr<KWayland::Client::DataSource> firstDataSource(firstConnection->dataDeviceManager->createDataSource());
    firstDataSource->offer(plainText());
    connect(firstDataSource.get(), &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto firstSurface = Test::createSurface(firstConnection->compositor);
    auto firstShellSurface = Test::createXdgToplevelSurface(firstConnection->xdgShell, firstSurface.get());
    auto firstWindow = Test::renderAndWaitForShown(firstConnection->shm, firstSurface.get(), QSize(100, 100), Qt::red);

    // Setup the second client.
    auto secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager);
    QVERIFY(Test::waitForWaylandKeyboard(secondConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> secondKeyboard(secondConnection->seat->createKeyboard());
    std::unique_ptr<KWayland::Client::DataDevice> secondDataDevice(secondConnection->dataDeviceManager->getDataDevice(secondConnection->seat));

    auto secondSurface = Test::createSurface(secondConnection->compositor);
    auto secondShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, secondSurface.get());
    auto secondWindow = Test::renderAndWaitForShown(secondConnection->shm, secondSurface.get(), QSize(100, 100), Qt::blue);

    auto thirdSurface = Test::createSurface(secondConnection->compositor);
    auto thirdShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, thirdSurface.get());
    auto thirdWindow = Test::renderAndWaitForShown(secondConnection->shm, thirdSurface.get(), QSize(100, 100), Qt::green);

    // Activate the first window and set the selection.
    QSignalSpy firstKeyboardEnteredSpy(firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstKeyboardEnteredSpy.wait());
    const quint32 firstEnteredSerial = firstKeyboardEnteredSpy.last().at(0).value<quint32>();
    firstDataDevice->setSelection(firstEnteredSerial, firstDataSource.get());

    QSignalSpy firstDataDeviceSelectionOfferedSpy(firstDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QSignalSpy firstDataDeviceSelectionClearedSpy(firstDataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);
    QSignalSpy secondDataDeviceSelectionOfferedSpy(secondDataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QSignalSpy secondDataDeviceSelectionClearedSpy(secondDataDevice.get(), &KWayland::Client::DataDevice::selectionCleared);

    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window.
    workspace()->activateWindow(secondWindow);
    QVERIFY(secondDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 3rd window (the 2nd and the 3rd window belong to the same client).
    workspace()->activateWindow(thirdWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window (the 2nd and the 3rd window belong to the same client).
    workspace()->activateWindow(secondWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 1st window (different connection from the 2nd and 3rd window).
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Clear the selection.
    firstDataDevice->setSelection(firstEnteredSerial, nullptr);
    QVERIFY(firstDataDeviceSelectionClearedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window (different connection from the 1st window).
    workspace()->activateWindow(secondWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 1); // 1 because a null selection is sent
}

void SelectionTest::primarySelection()
{
    // This test verifies that the primary selection works as expected between two clients.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);

    // Setup the source side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> sourceDataDevice(sourceConnection->primarySelectionManager->getDevice(sourceConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> dataSource(sourceConnection->primarySelectionManager->createSource());
    dataSource->offer(plainText().name());
    connect(dataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> targetDataDevice(targetConnection->primarySelectionManager->getDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the source window.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    sourceDataDevice->set_selection(dataSource->object(), sourceEnteredSerial);

    // The target device shouldn't receive an offer while it's not focused.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    QVERIFY(!targetDataDeviceSelectionOfferedSpy.wait(100));

    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = targetDataDevice->offer();
    QCOMPARE(offer->mimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(offer, htmlText());
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Clear selection.
    QSignalSpy targetDataDeviceSelectionClearedSpy(targetDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    sourceDataDevice->set_selection(nullptr, sourceEnteredSerial);
    QVERIFY(targetDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::internalPrimarySelection()
{
    // This test verifies that the source client also receives a wp_primary_selection_offer_v1 for its own selection.

    auto connection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);

    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> dataDevice(connection->primarySelectionManager->getDevice(connection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> dataSource(connection->primarySelectionManager->createSource());
    dataSource->offer(plainText().name());
    connect(dataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto surface = Test::createSurface(connection->compositor);
    auto shellSurface = Test::createXdgToplevelSurface(connection->xdgShell, surface.get());
    auto window = Test::renderAndWaitForShown(connection->shm, surface.get(), QSize(100, 100), Qt::red);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(connection->seat->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(window);
    QVERIFY(keyboardEnteredSpy.wait());
    const quint32 enteredSerial = keyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    dataDevice->set_selection(dataSource->object(), enteredSerial);

    QSignalSpy dataDeviceSelectionOfferedSpy(dataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    QVERIFY(dataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = dataDevice->offer();
    QCOMPARE(offer->mimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // text/html hasn't been offered, so this should fail.
    const QFuture<QByteArray> htmlData = readMimeTypeData(offer, htmlText());
    QVERIFY(waitFuture(htmlData));
    QCOMPARE(htmlData.result(), QByteArray());

    // Clear selection.
    QSignalSpy selectionClearedSpy(dataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    dataDevice->set_selection(nullptr, enteredSerial);
    QVERIFY(selectionClearedSpy.wait());
}

void SelectionTest::destroyPrimarySelection()
{
    // This test verifies that the wp_primary_selection_offer_v1 will be withdrawn if the associated data source is destroyed.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);

    // Setup the source side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> sourceDataDevice(sourceConnection->primarySelectionManager->getDevice(sourceConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> dataSource(sourceConnection->primarySelectionManager->createSource());
    dataSource->offer(plainText().name());
    connect(dataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> targetDataDevice(targetConnection->primarySelectionManager->getDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the source window.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    sourceDataDevice->set_selection(dataSource->object(), sourceEnteredSerial);

    // Activate the target window and wait for the data offer.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = targetDataDevice->offer();
    QCOMPARE(offer->mimeTypes(), QList<QMimeType>{plainText()});

    // Destroy the data source.
    QSignalSpy targetDataDeviceSelectionClearedSpy(targetDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    dataSource.reset();
    QVERIFY(targetDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::invalidSerialForPrimarySelection()
{
    // This test verifies that a set_selection request will be ignored if the specified serial is invalid.

    auto connection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);

    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> dataDevice(connection->primarySelectionManager->getDevice(connection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> dataSource(connection->primarySelectionManager->createSource());
    dataSource->offer(QStringLiteral("text/plain"));
    connect(dataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto surface = Test::createSurface(connection->compositor);
    auto shellSurface = Test::createXdgToplevelSurface(connection->xdgShell, surface.get());
    Test::renderAndWaitForShown(connection->shm, surface.get(), QSize(100, 100), Qt::red);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(connection->seat->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.wait());
    const quint32 enteredSerial = keyboardEnteredSpy.last().at(0).value<quint32>();

    // Set the selection.
    QSignalSpy dataSourceCancelledSpy(dataSource.get(), &Test::WpPrimarySelectionSourceV1::cancelled);
    dataDevice->set_selection(dataSource->object(), enteredSerial - 100);
    QVERIFY(dataSourceCancelledSpy.wait());
}

void SelectionTest::unsetSupersededPrimarySelection()
{
    // This test verifies that the current selection won't be unset if the previous selection is unset with set_selection(null).

    // Setup the first client.
    auto firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    QVERIFY(Test::waitForWaylandKeyboard(firstConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> firstKeyboard(firstConnection->seat->createKeyboard());
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> firstDataDevice(firstConnection->primarySelectionManager->getDevice(firstConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> firstDataSource(firstConnection->primarySelectionManager->createSource());
    firstDataSource->offer(plainText().name());
    connect(firstDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto firstSurface = Test::createSurface(firstConnection->compositor);
    auto firstShellSurface = Test::createXdgToplevelSurface(firstConnection->xdgShell, firstSurface.get());
    auto firstWindow = Test::renderAndWaitForShown(firstConnection->shm, firstSurface.get(), QSize(100, 100), Qt::red);

    // Setup the second client.
    auto secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    QVERIFY(Test::waitForWaylandKeyboard(secondConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> secondKeyboard(secondConnection->seat->createKeyboard());
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> secondDataDevice(secondConnection->primarySelectionManager->getDevice(secondConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> secondDataSource(secondConnection->primarySelectionManager->createSource());
    secondDataSource->offer(htmlText().name());
    connect(secondDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("bar");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto secondSurface = Test::createSurface(secondConnection->compositor);
    auto secondShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, secondSurface.get());
    auto secondWindow = Test::renderAndWaitForShown(secondConnection->shm, secondSurface.get(), QSize(100, 100), Qt::blue);

    // Activate the first window and set the selection.
    QSignalSpy firstKeyboardEnteredSpy(firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstDataDeviceSelectionOfferedSpy(firstDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstKeyboardEnteredSpy.wait());

    const quint32 firstEnteredSerial = firstKeyboardEnteredSpy.last().at(0).value<quint32>();
    firstDataDevice->set_selection(firstDataSource->object(), firstEnteredSerial);
    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    // Activate the second window and set the selection.
    QSignalSpy secondKeyboardEnteredSpy(secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondDataDeviceSelectionOfferedSpy(secondDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(secondWindow);
    QVERIFY(secondKeyboardEnteredSpy.wait());

    const quint32 secondEnteredSerial = secondKeyboardEnteredSpy.last().at(0).value<quint32>();
    secondDataDevice->set_selection(secondDataSource->object(), secondEnteredSerial);
    QVERIFY(secondDataDeviceSelectionOfferedSpy.wait());

    // Attempt to unset the first selection.
    QSignalSpy secondDataDeviceSelectionClearedSpy(secondDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    firstDataDevice->set_selection(nullptr, firstEnteredSerial);
    QVERIFY(!secondDataDeviceSelectionClearedSpy.wait(100));

    // Attempt to unset the second selection.
    secondDataDevice->set_selection(nullptr, secondEnteredSerial);
    QVERIFY(secondDataDeviceSelectionClearedSpy.wait());
}

void SelectionTest::receiveFromWithdrawnPrimarySelectionOffer()
{
    // This test verifies that a client cannot receive data from a withdrawn primary selection offer.

    auto sourceConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    auto targetConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);

    // Setup the source side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> sourceDataDevice(sourceConnection->primarySelectionManager->getDevice(sourceConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> dataSource(sourceConnection->primarySelectionManager->createSource());
    dataSource->offer(plainText().name());
    connect(dataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto sourceSurface = Test::createSurface(sourceConnection->compositor);
    auto sourceShellSurface = Test::createXdgToplevelSurface(sourceConnection->xdgShell, sourceSurface.get());
    auto sourceWindow = Test::renderAndWaitForShown(sourceConnection->shm, sourceSurface.get(), QSize(100, 100), Qt::red);

    // Setup the target side.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> targetDataDevice(targetConnection->primarySelectionManager->getDevice(targetConnection->seat));

    auto targetSurface = Test::createSurface(targetConnection->compositor);
    auto targetShellSurface = Test::createXdgToplevelSurface(targetConnection->xdgShell, targetSurface.get());
    auto targetWindow = Test::renderAndWaitForShown(targetConnection->shm, targetSurface.get(), QSize(100, 100), Qt::blue);

    // Focus the source window, and set the primary selection.
    std::unique_ptr<KWayland::Client::Keyboard> sourceKeyboard(sourceConnection->seat->createKeyboard());
    QSignalSpy sourceKeyboardEnteredSpy(sourceKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(sourceWindow);
    QVERIFY(sourceKeyboardEnteredSpy.wait());
    const quint32 sourceEnteredSerial = sourceKeyboardEnteredSpy.last().at(0).value<quint32>();
    sourceDataDevice->set_selection(dataSource->object(), sourceEnteredSerial);

    // The target client should be offered the selection when its window gets focused.
    QSignalSpy targetDataDeviceSelectionOfferedSpy(targetDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(targetWindow);
    QVERIFY(targetDataDeviceSelectionOfferedSpy.wait());
    std::unique_ptr<Test::WpPrimarySelectionOfferV1> offer = targetDataDevice->takeOffer();
    QCOMPARE(offer->mimeTypes(), QList<QMimeType>{plainText()});

    // Ask for data.
    const QFuture<QByteArray> plainData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(plainData));
    QCOMPARE(plainData.result(), QByteArrayLiteral("foo"));

    // Remove focus from the target window, it should not be able to receive data anymore.
    workspace()->activateWindow(nullptr);

    const QFuture<QByteArray> emptyData = readMimeTypeData(offer, plainText());
    QVERIFY(waitFuture(emptyData));
    QCOMPARE(emptyData.result(), QByteArray());
}

void SelectionTest::singlePrimarySelectionPerClient()
{
    // This test verifies that only one selection event will be sent when switching between two
    // surfaces that belong to the same client.

    // Setup the first client.
    auto firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    QVERIFY(Test::waitForWaylandKeyboard(firstConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> firstKeyboard(firstConnection->seat->createKeyboard());
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> firstDataDevice(firstConnection->primarySelectionManager->getDevice(firstConnection->seat));
    std::unique_ptr<Test::WpPrimarySelectionSourceV1> firstDataSource(firstConnection->primarySelectionManager->createSource());
    firstDataSource->offer(plainText().name());
    connect(firstDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &mimeType, int fd) {
        const auto data = QByteArrayLiteral("foo");
        write(fd, data.data(), data.size());
        close(fd);
    });

    auto firstSurface = Test::createSurface(firstConnection->compositor);
    auto firstShellSurface = Test::createXdgToplevelSurface(firstConnection->xdgShell, firstSurface.get());
    auto firstWindow = Test::renderAndWaitForShown(firstConnection->shm, firstSurface.get(), QSize(100, 100), Qt::red);

    // Setup the second client.
    auto secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::WpPrimarySelectionV1);
    QVERIFY(Test::waitForWaylandKeyboard(secondConnection->seat));

    std::unique_ptr<KWayland::Client::Keyboard> secondKeyboard(secondConnection->seat->createKeyboard());
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> secondDataDevice(secondConnection->primarySelectionManager->getDevice(secondConnection->seat));

    auto secondSurface = Test::createSurface(secondConnection->compositor);
    auto secondShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, secondSurface.get());
    auto secondWindow = Test::renderAndWaitForShown(secondConnection->shm, secondSurface.get(), QSize(100, 100), Qt::blue);

    auto thirdSurface = Test::createSurface(secondConnection->compositor);
    auto thirdShellSurface = Test::createXdgToplevelSurface(secondConnection->xdgShell, thirdSurface.get());
    auto thirdWindow = Test::renderAndWaitForShown(secondConnection->shm, thirdSurface.get(), QSize(100, 100), Qt::green);

    // Activate the first window and set the selection.
    QSignalSpy firstKeyboardEnteredSpy(firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstKeyboardEnteredSpy.wait());
    const quint32 firstEnteredSerial = firstKeyboardEnteredSpy.last().at(0).value<quint32>();
    firstDataDevice->set_selection(firstDataSource->object(), firstEnteredSerial);

    QSignalSpy firstDataDeviceSelectionOfferedSpy(firstDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    QSignalSpy firstDataDeviceSelectionClearedSpy(firstDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    QSignalSpy secondDataDeviceSelectionOfferedSpy(secondDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    QSignalSpy secondDataDeviceSelectionClearedSpy(secondDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);

    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window.
    workspace()->activateWindow(secondWindow);
    QVERIFY(secondDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 3rd window (the 2nd and the 3rd window belong to the same client).
    workspace()->activateWindow(thirdWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window (the 2nd and the 3rd window belong to the same client).
    workspace()->activateWindow(secondWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 1st window (different connection from the 2nd and 3rd window).
    workspace()->activateWindow(firstWindow);
    QVERIFY(firstDataDeviceSelectionOfferedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 0);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Clear the selection.
    firstDataDevice->set_selection(nullptr, firstEnteredSerial);
    QVERIFY(firstDataDeviceSelectionClearedSpy.wait());

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 0);

    // Activate the 2nd window (different connection from the 1st window).
    workspace()->activateWindow(secondWindow);
    QVERIFY(!secondDataDeviceSelectionOfferedSpy.wait(100));

    QCOMPARE(firstDataDeviceSelectionOfferedSpy.count(), 2);
    QCOMPARE(firstDataDeviceSelectionClearedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionOfferedSpy.count(), 1);
    QCOMPARE(secondDataDeviceSelectionClearedSpy.count(), 1); // 1 because a null selection is sent
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::SelectionTest)
#include "selection_test.moc"
