/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "debug_console.h"
#include "internalwindow.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <QPainter>
#include <QRasterWindow>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_debug_console-0");

class DebugConsoleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void topLevelTest_data();
    void topLevelTest();
    void testX11Window();
    void testX11Unmanaged();
    void testWaylandClient();
    void testInternalWindow();
    void testClosingDebugConsole();
};

void DebugConsoleTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::InternalWindow *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void DebugConsoleTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DebugConsoleTest::topLevelTest_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<int>("column");
    QTest::addColumn<bool>("expectedValid");

    // this tests various combinations of row/column on the top level whether they are valid
    // valid are rows 0-4 with column 0, everything else is invalid
    QTest::newRow("0/0") << 0 << 0 << true;
    QTest::newRow("0/1") << 0 << 1 << false;
    QTest::newRow("0/3") << 0 << 3 << false;
    QTest::newRow("1/0") << 1 << 0 << true;
    QTest::newRow("1/1") << 1 << 1 << false;
    QTest::newRow("1/3") << 1 << 3 << false;
    QTest::newRow("2/0") << 2 << 0 << true;
    QTest::newRow("3/0") << 3 << 0 << true;
    QTest::newRow("4/0") << 4 << 0 << false;
    QTest::newRow("100/0") << 4 << 0 << false;
}

void DebugConsoleTest::topLevelTest()
{
    DebugConsoleModel model;
    QCOMPARE(model.rowCount(QModelIndex()), 4);
    QCOMPARE(model.columnCount(QModelIndex()), 2);
    QFETCH(int, row);
    QFETCH(int, column);
    const QModelIndex index = model.index(row, column, QModelIndex());
    QTEST(index.isValid(), "expectedValid");
    if (index.isValid()) {
        QVERIFY(!model.parent(index).isValid());
        QVERIFY(model.data(index, Qt::DisplayRole).isValid());
        QCOMPARE(model.data(index, Qt::DisplayRole).userType(), int(QMetaType::QString));
        for (int i = Qt::DecorationRole; i <= Qt::UserRole; i++) {
            QVERIFY(!model.data(index, i).isValid());
        }
    }
}

void DebugConsoleTest::testX11Window()
{
    DebugConsoleModel model;
    QModelIndex x11TopLevelIndex = model.index(0, 0, QModelIndex());
    QVERIFY(x11TopLevelIndex.isValid());
    // we don't have any windows yet
    QCOMPARE(model.rowCount(x11TopLevelIndex), 0);
    QVERIFY(!model.hasChildren(x11TopLevelIndex));
    // child index must be invalid
    QVERIFY(!model.index(0, 0, x11TopLevelIndex).isValid());
    QVERIFY(!model.index(0, 1, x11TopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, x11TopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, x11TopLevelIndex).isValid());

    // start glxgears, to get a window, which should be added to the model
    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);

    QProcess glxgears;
    glxgears.setProgram(QStringLiteral("glxgears"));
    glxgears.start();
    QVERIFY(glxgears.waitForStarted());

    QVERIFY(rowsInsertedSpy.wait());
    QCOMPARE(rowsInsertedSpy.count(), 1);
    QVERIFY(model.hasChildren(x11TopLevelIndex));
    QCOMPARE(model.rowCount(x11TopLevelIndex), 1);
    QCOMPARE(rowsInsertedSpy.first().at(0).value<QModelIndex>(), x11TopLevelIndex);
    QCOMPARE(rowsInsertedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsInsertedSpy.first().at(2).value<int>(), 0);

    QModelIndex windowIndex = model.index(0, 0, x11TopLevelIndex);
    QVERIFY(windowIndex.isValid());
    QCOMPARE(model.parent(windowIndex), x11TopLevelIndex);
    QVERIFY(model.hasChildren(windowIndex));
    QVERIFY(model.rowCount(windowIndex) != 0);
    QCOMPARE(model.columnCount(windowIndex), 2);
    // other indexes are still invalid
    QVERIFY(!model.index(0, 1, x11TopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, x11TopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, x11TopLevelIndex).isValid());

    // the windowIndex has children and those are properties
    for (int i = 0; i < model.rowCount(windowIndex); i++) {
        const QModelIndex propNameIndex = model.index(i, 0, windowIndex);
        QVERIFY(propNameIndex.isValid());
        QCOMPARE(model.parent(propNameIndex), windowIndex);
        QVERIFY(!model.hasChildren(propNameIndex));
        QVERIFY(!model.index(0, 0, propNameIndex).isValid());
        QVERIFY(model.data(propNameIndex, Qt::DisplayRole).isValid());
        QCOMPARE(model.data(propNameIndex, Qt::DisplayRole).userType(), int(QMetaType::QString));

        // and the value
        const QModelIndex propValueIndex = model.index(i, 1, windowIndex);
        QVERIFY(propValueIndex.isValid());
        QCOMPARE(model.parent(propValueIndex), windowIndex);
        QVERIFY(!model.index(0, 0, propValueIndex).isValid());
        QVERIFY(!model.hasChildren(propValueIndex));
        // TODO: how to test whether the values actually work?

        // and on third column we should not get an index any more
        QVERIFY(!model.index(i, 2, windowIndex).isValid());
    }
    // row after count should be invalid
    QVERIFY(!model.index(model.rowCount(windowIndex), 0, windowIndex).isValid());

    // creating a second model should be initialized directly with the X11 child
    DebugConsoleModel model2;
    QVERIFY(model2.hasChildren(model2.index(0, 0, QModelIndex())));

    // now close the window again, it should be removed from the model
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);

    glxgears.terminate();
    QVERIFY(glxgears.waitForFinished());

    QVERIFY(rowsRemovedSpy.wait());
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.first().first().value<QModelIndex>(), x11TopLevelIndex);
    QCOMPARE(rowsRemovedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsRemovedSpy.first().at(2).value<int>(), 0);

    // the child should be gone again
    QVERIFY(!model.hasChildren(x11TopLevelIndex));
    QVERIFY(!model2.hasChildren(model2.index(0, 0, QModelIndex())));
}

void DebugConsoleTest::testX11Unmanaged()
{
    DebugConsoleModel model;
    QModelIndex unmanagedTopLevelIndex = model.index(1, 0, QModelIndex());
    QVERIFY(unmanagedTopLevelIndex.isValid());
    // we don't have any windows yet
    QCOMPARE(model.rowCount(unmanagedTopLevelIndex), 0);
    QVERIFY(!model.hasChildren(unmanagedTopLevelIndex));
    // child index must be invalid
    QVERIFY(!model.index(0, 0, unmanagedTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 1, unmanagedTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, unmanagedTopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, unmanagedTopLevelIndex).isValid());

    // we need to create an unmanaged window
    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);

    // let's create an override redirect window
    const uint32_t values[] = {true};
    Xcb::Window window(QRect(0, 0, 10, 10), XCB_CW_OVERRIDE_REDIRECT, values);
    window.map();

    QVERIFY(rowsInsertedSpy.wait());
    QCOMPARE(rowsInsertedSpy.count(), 1);
    QVERIFY(model.hasChildren(unmanagedTopLevelIndex));
    QCOMPARE(model.rowCount(unmanagedTopLevelIndex), 1);
    QCOMPARE(rowsInsertedSpy.first().at(0).value<QModelIndex>(), unmanagedTopLevelIndex);
    QCOMPARE(rowsInsertedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsInsertedSpy.first().at(2).value<int>(), 0);

    QModelIndex windowIndex = model.index(0, 0, unmanagedTopLevelIndex);
    QVERIFY(windowIndex.isValid());
    QCOMPARE(model.parent(windowIndex), unmanagedTopLevelIndex);
    QVERIFY(model.hasChildren(windowIndex));
    QVERIFY(model.rowCount(windowIndex) != 0);
    QCOMPARE(model.columnCount(windowIndex), 2);
    // other indexes are still invalid
    QVERIFY(!model.index(0, 1, unmanagedTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, unmanagedTopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, unmanagedTopLevelIndex).isValid());

    QCOMPARE(model.data(windowIndex, Qt::DisplayRole).toString(), QStringLiteral("0x%1").arg(window, 0, 16));

    // the windowIndex has children and those are properties
    for (int i = 0; i < model.rowCount(windowIndex); i++) {
        const QModelIndex propNameIndex = model.index(i, 0, windowIndex);
        QVERIFY(propNameIndex.isValid());
        QCOMPARE(model.parent(propNameIndex), windowIndex);
        QVERIFY(!model.hasChildren(propNameIndex));
        QVERIFY(!model.index(0, 0, propNameIndex).isValid());
        QVERIFY(model.data(propNameIndex, Qt::DisplayRole).isValid());
        QCOMPARE(model.data(propNameIndex, Qt::DisplayRole).userType(), int(QMetaType::QString));

        // and the value
        const QModelIndex propValueIndex = model.index(i, 1, windowIndex);
        QVERIFY(propValueIndex.isValid());
        QCOMPARE(model.parent(propValueIndex), windowIndex);
        QVERIFY(!model.index(0, 0, propValueIndex).isValid());
        QVERIFY(!model.hasChildren(propValueIndex));
        // TODO: how to test whether the values actually work?

        // and on third column we should not get an index any more
        QVERIFY(!model.index(i, 2, windowIndex).isValid());
    }
    // row after count should be invalid
    QVERIFY(!model.index(model.rowCount(windowIndex), 0, windowIndex).isValid());

    // creating a second model should be initialized directly with the X11 child
    DebugConsoleModel model2;
    QVERIFY(model2.hasChildren(model2.index(1, 0, QModelIndex())));

    // now close the window again, it should be removed from the model
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);

    window.unmap();

    QVERIFY(rowsRemovedSpy.wait());
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.first().first().value<QModelIndex>(), unmanagedTopLevelIndex);
    QCOMPARE(rowsRemovedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsRemovedSpy.first().at(2).value<int>(), 0);

    // the child should be gone again
    QVERIFY(!model.hasChildren(unmanagedTopLevelIndex));
    QVERIFY(!model2.hasChildren(model2.index(1, 0, QModelIndex())));
}

void DebugConsoleTest::testWaylandClient()
{
    DebugConsoleModel model;
    QModelIndex waylandTopLevelIndex = model.index(2, 0, QModelIndex());
    QVERIFY(waylandTopLevelIndex.isValid());

    // we don't have any windows yet
    QCOMPARE(model.rowCount(waylandTopLevelIndex), 0);
    QVERIFY(!model.hasChildren(waylandTopLevelIndex));
    // child index must be invalid
    QVERIFY(!model.index(0, 0, waylandTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 1, waylandTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, waylandTopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, waylandTopLevelIndex).isValid());

    // we need to create a wayland window
    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);

    // create our connection
    QVERIFY(Test::setupWaylandConnection());

    // create the Surface and ShellSurface
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface->isValid());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Test::render(surface.get(), QSize(10, 10), Qt::red);

    // now we have the window, it should be added to our model
    QVERIFY(rowsInsertedSpy.wait());
    QCOMPARE(rowsInsertedSpy.count(), 1);

    QVERIFY(model.hasChildren(waylandTopLevelIndex));
    QCOMPARE(model.rowCount(waylandTopLevelIndex), 1);
    QCOMPARE(rowsInsertedSpy.first().at(0).value<QModelIndex>(), waylandTopLevelIndex);
    QCOMPARE(rowsInsertedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsInsertedSpy.first().at(2).value<int>(), 0);

    QModelIndex windowIndex = model.index(0, 0, waylandTopLevelIndex);
    QVERIFY(windowIndex.isValid());
    QCOMPARE(model.parent(windowIndex), waylandTopLevelIndex);
    QVERIFY(model.hasChildren(windowIndex));
    QVERIFY(model.rowCount(windowIndex) != 0);
    QCOMPARE(model.columnCount(windowIndex), 2);
    // other indexes are still invalid
    QVERIFY(!model.index(0, 1, waylandTopLevelIndex).isValid());
    QVERIFY(!model.index(0, 2, waylandTopLevelIndex).isValid());
    QVERIFY(!model.index(1, 0, waylandTopLevelIndex).isValid());

    // the windowIndex has children and those are properties
    for (int i = 0; i < model.rowCount(windowIndex); i++) {
        const QModelIndex propNameIndex = model.index(i, 0, windowIndex);
        QVERIFY(propNameIndex.isValid());
        QCOMPARE(model.parent(propNameIndex), windowIndex);
        QVERIFY(!model.hasChildren(propNameIndex));
        QVERIFY(!model.index(0, 0, propNameIndex).isValid());
        QVERIFY(model.data(propNameIndex, Qt::DisplayRole).isValid());
        QCOMPARE(model.data(propNameIndex, Qt::DisplayRole).userType(), int(QMetaType::QString));

        // and the value
        const QModelIndex propValueIndex = model.index(i, 1, windowIndex);
        QVERIFY(propValueIndex.isValid());
        QCOMPARE(model.parent(propValueIndex), windowIndex);
        QVERIFY(!model.index(0, 0, propValueIndex).isValid());
        QVERIFY(!model.hasChildren(propValueIndex));
        // TODO: how to test whether the values actually work?

        // and on third column we should not get an index any more
        QVERIFY(!model.index(i, 2, windowIndex).isValid());
    }
    // row after count should be invalid
    QVERIFY(!model.index(model.rowCount(windowIndex), 0, windowIndex).isValid());

    // creating a second model should be initialized directly with the X11 child
    DebugConsoleModel model2;
    QVERIFY(model2.hasChildren(model2.index(2, 0, QModelIndex())));

    // now close the window again, it should be removed from the model
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);

    surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(rowsRemovedSpy.wait());

    QCOMPARE(rowsRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.first().first().value<QModelIndex>(), waylandTopLevelIndex);
    QCOMPARE(rowsRemovedSpy.first().at(1).value<int>(), 0);
    QCOMPARE(rowsRemovedSpy.first().at(2).value<int>(), 0);

    // the child should be gone again
    QVERIFY(!model.hasChildren(waylandTopLevelIndex));
    QVERIFY(!model2.hasChildren(model2.index(2, 0, QModelIndex())));
}

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow()
        : QRasterWindow(nullptr)
    {
    }
    ~HelperWindow() override = default;

Q_SIGNALS:
    void entered();
    void left();
    void mouseMoved(const QPoint &global);
    void mousePressed();
    void mouseReleased();
    void wheel();
    void keyPressed();
    void keyReleased();

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), Qt::red);
    }
};

void DebugConsoleTest::testInternalWindow()
{
    DebugConsoleModel model;
    QModelIndex internalTopLevelIndex = model.index(3, 0, QModelIndex());
    QVERIFY(internalTopLevelIndex.isValid());

    // there might already be some internal windows, so we cannot reliable test whether there are children
    // given that we just test whether adding a window works.

    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);

    std::unique_ptr<HelperWindow> w(new HelperWindow);
    w->setGeometry(0, 0, 100, 100);
    w->show();

    QTRY_COMPARE(rowsInsertedSpy.count(), 1);
    QCOMPARE(rowsInsertedSpy.first().first().value<QModelIndex>(), internalTopLevelIndex);

    QModelIndex windowIndex = model.index(rowsInsertedSpy.first().last().toInt(), 0, internalTopLevelIndex);
    QVERIFY(windowIndex.isValid());
    QCOMPARE(model.parent(windowIndex), internalTopLevelIndex);
    QVERIFY(model.hasChildren(windowIndex));
    QVERIFY(model.rowCount(windowIndex) != 0);
    QCOMPARE(model.columnCount(windowIndex), 2);
    // other indexes are still invalid
    QVERIFY(!model.index(rowsInsertedSpy.first().last().toInt(), 1, internalTopLevelIndex).isValid());
    QVERIFY(!model.index(rowsInsertedSpy.first().last().toInt(), 2, internalTopLevelIndex).isValid());
    QVERIFY(!model.index(rowsInsertedSpy.first().last().toInt() + 1, 0, internalTopLevelIndex).isValid());

    // the wayland shell client top level should not have gained this window
    QVERIFY(!model.hasChildren(model.index(2, 0, QModelIndex())));

    // the windowIndex has children and those are properties
    for (int i = 0; i < model.rowCount(windowIndex); i++) {
        const QModelIndex propNameIndex = model.index(i, 0, windowIndex);
        QVERIFY(propNameIndex.isValid());
        QCOMPARE(model.parent(propNameIndex), windowIndex);
        QVERIFY(!model.hasChildren(propNameIndex));
        QVERIFY(!model.index(0, 0, propNameIndex).isValid());
        QVERIFY(model.data(propNameIndex, Qt::DisplayRole).isValid());
        QCOMPARE(model.data(propNameIndex, Qt::DisplayRole).userType(), int(QMetaType::QString));

        // and the value
        const QModelIndex propValueIndex = model.index(i, 1, windowIndex);
        QVERIFY(propValueIndex.isValid());
        QCOMPARE(model.parent(propValueIndex), windowIndex);
        QVERIFY(!model.index(0, 0, propValueIndex).isValid());
        QVERIFY(!model.hasChildren(propValueIndex));
        // TODO: how to test whether the values actually work?

        // and on third column we should not get an index any more
        QVERIFY(!model.index(i, 2, windowIndex).isValid());
    }
    // row after count should be invalid
    QVERIFY(!model.index(model.rowCount(windowIndex), 0, windowIndex).isValid());

    // now close the window again, it should be removed from the model
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);

    w->hide();
    w.reset();

    QTRY_COMPARE(rowsRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.first().first().value<QModelIndex>(), internalTopLevelIndex);
}

void DebugConsoleTest::testClosingDebugConsole()
{
    // this test verifies that the DebugConsole gets destroyed when closing the window
    // BUG: 369858

    DebugConsole *console = new DebugConsole;
    QSignalSpy destroyedSpy(console, &QObject::destroyed);

    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    console->show();
    QCOMPARE(console->windowHandle()->isVisible(), true);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    InternalWindow *window = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(window->isInternal());
    QCOMPARE(window->handle(), console->windowHandle());
    QVERIFY(window->isDecorated());
    QCOMPARE(window->isMinimizable(), false);
    window->closeWindow();
    QVERIFY(destroyedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::DebugConsoleTest)
#include "debug_console_test.moc"
