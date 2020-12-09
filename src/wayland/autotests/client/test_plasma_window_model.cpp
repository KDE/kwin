/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/plasmawindowmanagement.h"
#include "KWayland/Client/plasmawindowmodel.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/plasmawindowmanagement_interface.h"
#include "../../src/server/plasmavirtualdesktop_interface.h"

#include <linux/input.h>

using namespace KWayland::Client;
using namespace KWaylandServer;

Q_DECLARE_METATYPE(Qt::MouseButton)
typedef void (KWayland::Client::PlasmaWindow::*ClientWindowSignal)();
Q_DECLARE_METATYPE(ClientWindowSignal)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowBoolSetter)(bool);
Q_DECLARE_METATYPE(ServerWindowBoolSetter)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowStringSetter)(const QString&);
Q_DECLARE_METATYPE(ServerWindowStringSetter)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowQuint32Setter)(quint32);
Q_DECLARE_METATYPE(ServerWindowQuint32Setter)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowVoidSetter)();
Q_DECLARE_METATYPE(ServerWindowVoidSetter)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowIconSetter)(const QIcon&);
Q_DECLARE_METATYPE(ServerWindowIconSetter)

class PlasmaWindowModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testRoleNames_data();
    void testRoleNames();
    void testAddRemoveRows();
    void testDefaultData_data();
    void testDefaultData();
    void testIsActive();
    void testIsFullscreenable();
    void testIsFullscreen();
    void testIsMaximizable();
    void testIsMaximized();
    void testIsMinimizable();
    void testIsMinimized();
    void testIsKeepAbove();
    void testIsKeepBelow();
    void testIsDemandingAttention();
    void testSkipTaskbar();
    void testSkipSwitcher();
    void testIsShadeable();
    void testIsShaded();
    void testIsMovable();
    void testIsResizable();
    void testIsVirtualDesktopChangeable();
    void testIsCloseable();
    void testGeometry();
    void testTitle();
    void testAppId();
    void testPid();
    void testVirtualDesktops();
    // TODO icon: can we ensure a theme is installed on CI?
    void testRequests();
    // TODO: minimized geometry
    // TODO: model reset
    void testCreateWithUnmappedWindow();
    void testChangeWindowAfterModelDestroy_data();
    void testChangeWindowAfterModelDestroy();
    void testCreateWindowAfterModelDestroy();

private:
    bool testBooleanData(PlasmaWindowModel::AdditionalRoles role, void (PlasmaWindowInterface::*function)(bool));
    Display *m_display = nullptr;
    PlasmaWindowManagementInterface *m_pwInterface = nullptr;
    PlasmaWindowManagement *m_pw = nullptr;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_plasmaVirtualDesktopManagementInterface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-fake-input-0");

void PlasmaWindowModelTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_pwInterface = new PlasmaWindowManagementInterface(m_display, m_display);
    m_plasmaVirtualDesktopManagementInterface = new PlasmaVirtualDesktopManagementInterface(m_display, m_display);
    m_plasmaVirtualDesktopManagementInterface->createDesktop("desktop1");
    m_plasmaVirtualDesktopManagementInterface->createDesktop("desktop2");
    m_pwInterface->setPlasmaVirtualDesktopManagementInterface(m_plasmaVirtualDesktopManagementInterface);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_pw = registry.createPlasmaWindowManagement(registry.interface(Registry::Interface::PlasmaWindowManagement).name,
                                                 registry.interface(Registry::Interface::PlasmaWindowManagement).version,
                                                 this);
    QVERIFY(m_pw->isValid());
}

void PlasmaWindowModelTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_pw)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_plasmaVirtualDesktopManagementInterface = nullptr;
    m_pwInterface = nullptr;
}

bool PlasmaWindowModelTest::testBooleanData(PlasmaWindowModel::AdditionalRoles role, void (PlasmaWindowInterface::*function)(bool))
{
#define VERIFY(statement) \
if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
    return false;
#define COMPARE(actual, expected) \
if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
    return false;

    auto model = m_pw->createWindowModel();
    VERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    VERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    VERIFY(w);
    VERIFY(rowInsertedSpy.wait());
    m_connection->flush();
    m_display->dispatchEvents();
    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    VERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    COMPARE(model->data(index, role).toBool(), false);

    (w->*(function))(true);
    VERIFY(dataChangedSpy.wait());
    COMPARE(dataChangedSpy.count(), 1);
    COMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    COMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(role)});
    COMPARE(model->data(index, role).toBool(), true);

    (w->*(function))(false);
    VERIFY(dataChangedSpy.wait());
    COMPARE(dataChangedSpy.count(), 2);
    COMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    COMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(role)});
    COMPARE(model->data(index, role).toBool(), false);

#undef COMPARE
#undef VERIFY
    return true;
}

void PlasmaWindowModelTest::testRoleNames_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QByteArray>("name");

    QTest::newRow("display") << int(Qt::DisplayRole) << QByteArrayLiteral("DisplayRole");
    QTest::newRow("decoration") << int(Qt::DecorationRole) << QByteArrayLiteral("DecorationRole");

    QTest::newRow("AppId")                << int(PlasmaWindowModel::AppId) << QByteArrayLiteral("AppId");
    QTest::newRow("Pid")                  << int(PlasmaWindowModel::Pid) << QByteArrayLiteral("Pid");
    QTest::newRow("IsActive")             << int(PlasmaWindowModel::IsActive) << QByteArrayLiteral("IsActive");
    QTest::newRow("IsFullscreenable")     << int(PlasmaWindowModel::IsFullscreenable) << QByteArrayLiteral("IsFullscreenable");
    QTest::newRow("IsFullscreen")         << int(PlasmaWindowModel::IsFullscreen) << QByteArrayLiteral("IsFullscreen");
    QTest::newRow("IsMaximizable")        << int(PlasmaWindowModel::IsMaximizable) << QByteArrayLiteral("IsMaximizable");
    QTest::newRow("IsMaximized")          << int(PlasmaWindowModel::IsMaximized) << QByteArrayLiteral("IsMaximized");
    QTest::newRow("IsMinimizable")        << int(PlasmaWindowModel::IsMinimizable) << QByteArrayLiteral("IsMinimizable");
    QTest::newRow("IsMinimized")          << int(PlasmaWindowModel::IsMinimized) << QByteArrayLiteral("IsMinimized");
    QTest::newRow("IsKeepAbove")          << int(PlasmaWindowModel::IsKeepAbove) << QByteArrayLiteral("IsKeepAbove");
    QTest::newRow("IsKeepBelow")          << int(PlasmaWindowModel::IsKeepBelow) << QByteArrayLiteral("IsKeepBelow");
    QTest::newRow("VirtualDesktop")       << int(PlasmaWindowModel::VirtualDesktop) << QByteArrayLiteral("VirtualDesktop");
    QTest::newRow("IsOnAllDesktops")      << int(PlasmaWindowModel::IsOnAllDesktops) << QByteArrayLiteral("IsOnAllDesktops");
    QTest::newRow("IsDemandingAttention") << int(PlasmaWindowModel::IsDemandingAttention) << QByteArrayLiteral("IsDemandingAttention");
    QTest::newRow("SkipTaskbar")          << int(PlasmaWindowModel::SkipTaskbar) << QByteArrayLiteral("SkipTaskbar");
    QTest::newRow("SkipSwitcher")         << int(PlasmaWindowModel::SkipSwitcher) << QByteArrayLiteral("SkipSwitcher");
    QTest::newRow("IsShadeable")           << int(PlasmaWindowModel::IsShadeable) << QByteArrayLiteral("IsShadeable");
    QTest::newRow("IsShaded")             << int(PlasmaWindowModel::IsShaded) << QByteArrayLiteral("IsShaded");
    QTest::newRow("IsMovable")            << int(PlasmaWindowModel::IsMovable) << QByteArrayLiteral("IsMovable");
    QTest::newRow("IsResizable")          << int(PlasmaWindowModel::IsResizable) << QByteArrayLiteral("IsResizable");
    QTest::newRow("IsVirtualDesktopChangeable") << int(PlasmaWindowModel::IsVirtualDesktopChangeable) << QByteArrayLiteral("IsVirtualDesktopChangeable");
    QTest::newRow("IsCloseable")          << int(PlasmaWindowModel::IsCloseable) << QByteArrayLiteral("IsCloseable");
    QTest::newRow("Geometry")          << int(PlasmaWindowModel::Geometry) << QByteArrayLiteral("Geometry");
}

void PlasmaWindowModelTest::testRoleNames()
{
    // just verifies that all role names are available
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    const auto roles = model->roleNames();

    QFETCH(int, role);
    auto it = roles.find(role);
    QVERIFY(it != roles.end());
    QTEST(it.value(), "name");
}

void PlasmaWindowModelTest::testAddRemoveRows()
{
    // this test verifies that adding/removing rows to the Model works
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->index(0).isValid());

    // now let's add a row
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    // this happens by creating a PlasmaWindow on server side
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());
    QCOMPARE(rowInsertedSpy.count(), 1);
    QVERIFY(!rowInsertedSpy.first().at(0).toModelIndex().isValid());
    QCOMPARE(rowInsertedSpy.first().at(1).toInt(), 0);
    QCOMPARE(rowInsertedSpy.first().at(2).toInt(), 0);

    // the model should have a row now
    QCOMPARE(model->rowCount(), 1);
    QVERIFY(model->index(0).isValid());
    // that index doesn't have children
    QCOMPARE(model->rowCount(model->index(0)), 0);

    // process events in order to ensure that the resource is created on server side before we unmap the window
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);

    // now let's remove that again
    QSignalSpy rowRemovedSpy(model, &PlasmaWindowModel::rowsRemoved);
    QVERIFY(rowRemovedSpy.isValid());
    w->unmap();
    QVERIFY(rowRemovedSpy.wait());
    QCOMPARE(rowRemovedSpy.count(), 1);
    QVERIFY(!rowRemovedSpy.first().at(0).toModelIndex().isValid());
    QCOMPARE(rowRemovedSpy.first().at(1).toInt(), 0);
    QCOMPARE(rowRemovedSpy.first().at(2).toInt(), 0);

    // now the model is empty again
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->index(0).isValid());

    QSignalSpy wDestroyedSpy(w, &QObject::destroyed);
    QVERIFY(wDestroyedSpy.isValid());
    QVERIFY(wDestroyedSpy.wait());
}

void PlasmaWindowModelTest::testDefaultData_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("display") << int(Qt::DisplayRole) << QVariant(QString());
    QTest::newRow("decoration") << int(Qt::DecorationRole) << QVariant(QIcon());

    QTest::newRow("AppId")                << int(PlasmaWindowModel::AppId) << QVariant(QString());
    QTest::newRow("IsActive")             << int(PlasmaWindowModel::IsActive) << QVariant(false);
    QTest::newRow("IsFullscreenable")     << int(PlasmaWindowModel::IsFullscreenable) << QVariant(false);
    QTest::newRow("IsFullscreen")         << int(PlasmaWindowModel::IsFullscreen) << QVariant(false);
    QTest::newRow("IsMaximizable")        << int(PlasmaWindowModel::IsMaximizable) << QVariant(false);
    QTest::newRow("IsMaximized")          << int(PlasmaWindowModel::IsMaximized) << QVariant(false);
    QTest::newRow("IsMinimizable")        << int(PlasmaWindowModel::IsMinimizable) << QVariant(false);
    QTest::newRow("IsMinimized")          << int(PlasmaWindowModel::IsMinimized) << QVariant(false);
    QTest::newRow("IsKeepAbove")          << int(PlasmaWindowModel::IsKeepAbove) << QVariant(false);
    QTest::newRow("IsKeepBelow")          << int(PlasmaWindowModel::IsKeepBelow) << QVariant(false);
    QTest::newRow("VirtualDesktop")       << int(PlasmaWindowModel::VirtualDesktop) << QVariant(0);
    QTest::newRow("IsOnAllDesktops")      << int(PlasmaWindowModel::IsOnAllDesktops) << QVariant(true);
    QTest::newRow("IsDemandingAttention") << int(PlasmaWindowModel::IsDemandingAttention) << QVariant(false);
    QTest::newRow("IsShadeable")           << int(PlasmaWindowModel::IsShadeable) << QVariant(false);
    QTest::newRow("IsShaded")             << int(PlasmaWindowModel::IsShaded) << QVariant(false);
    QTest::newRow("SkipTaskbar")          << int(PlasmaWindowModel::SkipTaskbar) << QVariant(false);
    QTest::newRow("IsMovable")            << int(PlasmaWindowModel::IsMovable) << QVariant(false);
    QTest::newRow("IsResizable")          << int(PlasmaWindowModel::IsResizable) << QVariant(false);
    QTest::newRow("IsVirtualDesktopChangeable") << int(PlasmaWindowModel::IsVirtualDesktopChangeable) << QVariant(false);
    QTest::newRow("IsCloseable")          << int(PlasmaWindowModel::IsCloseable) << QVariant(false);
    QTest::newRow("Geometry")             << int(PlasmaWindowModel::Geometry) << QVariant(QRect());
    QTest::newRow("Pid")                  << int(PlasmaWindowModel::Pid) << QVariant(0);
}

void PlasmaWindowModelTest::testDefaultData()
{
    // this test validates the default data of a PlasmaWindow without having set any values
    // first create a model with a window
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());

    QModelIndex index = model->index(0);
    QFETCH(int, role);
    QTEST(model->data(index, role), "value");
}

void PlasmaWindowModelTest::testIsActive()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsActive, &PlasmaWindowInterface::setActive));
}

void PlasmaWindowModelTest::testIsFullscreenable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsFullscreenable, &PlasmaWindowInterface::setFullscreenable));
}

void PlasmaWindowModelTest::testIsFullscreen()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsFullscreen, &PlasmaWindowInterface::setFullscreen));
}

void PlasmaWindowModelTest::testIsMaximizable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsMaximizable, &PlasmaWindowInterface::setMaximizeable));
}

void PlasmaWindowModelTest::testIsMaximized()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsMaximized, &PlasmaWindowInterface::setMaximized));
}

void PlasmaWindowModelTest::testIsMinimizable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsMinimizable, &PlasmaWindowInterface::setMinimizeable));
}

void PlasmaWindowModelTest::testIsMinimized()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsMinimized, &PlasmaWindowInterface::setMinimized));
}

void PlasmaWindowModelTest::testIsKeepAbove()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsKeepAbove, &PlasmaWindowInterface::setKeepAbove));
}

void PlasmaWindowModelTest::testIsKeepBelow()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsKeepBelow, &PlasmaWindowInterface::setKeepBelow));
}

void PlasmaWindowModelTest::testIsDemandingAttention()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsDemandingAttention, &PlasmaWindowInterface::setDemandsAttention));
}

void PlasmaWindowModelTest::testSkipTaskbar()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::SkipTaskbar, &PlasmaWindowInterface::setSkipTaskbar));
}

void PlasmaWindowModelTest::testSkipSwitcher()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::SkipSwitcher, &PlasmaWindowInterface::setSkipSwitcher));
}

void PlasmaWindowModelTest::testIsShadeable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsShadeable, &PlasmaWindowInterface::setShadeable));
}

void PlasmaWindowModelTest::testIsShaded()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsShaded, &PlasmaWindowInterface::setShaded));
}

void PlasmaWindowModelTest::testIsMovable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsMovable, &PlasmaWindowInterface::setMovable));
}

void PlasmaWindowModelTest::testIsResizable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsResizable, &PlasmaWindowInterface::setResizable));
}

void PlasmaWindowModelTest::testIsVirtualDesktopChangeable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsVirtualDesktopChangeable, &PlasmaWindowInterface::setVirtualDesktopChangeable));
}

void PlasmaWindowModelTest::testIsCloseable()
{
    QVERIFY(testBooleanData(PlasmaWindowModel::IsCloseable, &PlasmaWindowInterface::setCloseable));
}

void PlasmaWindowModelTest::testGeometry()
{
    auto model = m_pw->createWindowModel();
    QVERIFY(model);

    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());

    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());

    const QModelIndex index = model->index(0);

    QCOMPARE(model->data(index, PlasmaWindowModel::Geometry).toRect(), QRect());

    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QRect geom(0, 15, 50, 75);
    w->setGeometry(geom);

    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::Geometry)});

    QCOMPARE(model->data(index, PlasmaWindowModel::Geometry).toRect(), geom);
}

void PlasmaWindowModelTest::testTitle()
{
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());
    m_connection->flush();
    m_display->dispatchEvents();
    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, Qt::DisplayRole).toString(), QString());

    w->setTitle(QStringLiteral("foo"));
    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(Qt::DisplayRole)});
    QCOMPARE(model->data(index, Qt::DisplayRole).toString(), QStringLiteral("foo"));
}

void PlasmaWindowModelTest::testAppId()
{
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());
    m_connection->flush();
    m_display->dispatchEvents();
    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, PlasmaWindowModel::AppId).toString(), QString());

    w->setAppId(QStringLiteral("org.kde.testapp"));
    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::AppId)});
    QCOMPARE(model->data(index, PlasmaWindowModel::AppId).toString(), QStringLiteral("org.kde.testapp"));
}

void PlasmaWindowModelTest::testPid()
{
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    w->setPid(1337);
    QVERIFY(w);
    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(rowInsertedSpy.wait());

    //pid should be set as soon as the new row appears
    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, PlasmaWindowModel::Pid).toInt(), 1337);
}

void PlasmaWindowModelTest::testVirtualDesktops()
{
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());
    m_connection->flush();
    m_display->dispatchEvents();
    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());

    const QModelIndex index = model->index(0);
    QCOMPARE(model->data(index, PlasmaWindowModel::VirtualDesktops).toStringList(), QStringList());

    w->addPlasmaVirtualDesktop("desktop1");
    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.count(), 2);
    QCOMPARE(dataChangedSpy.first().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);

    QCOMPARE(dataChangedSpy.first().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::VirtualDesktops)});
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::IsOnAllDesktops)});

    QCOMPARE(model->data(index, PlasmaWindowModel::VirtualDesktops).toStringList(), QStringList({"desktop1"}));
    QCOMPARE(model->data(index, PlasmaWindowModel::IsOnAllDesktops).toBool(), false);

    dataChangedSpy.clear();
    w->addPlasmaVirtualDesktop("desktop2");
    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(dataChangedSpy.last().first().toModelIndex(), index);
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::VirtualDesktops)});
    QCOMPARE(model->data(index, PlasmaWindowModel::VirtualDesktops).toStringList(), QStringList({"desktop1", "desktop2"}));
    QCOMPARE(model->data(index, PlasmaWindowModel::IsOnAllDesktops).toBool(), false);

    w->removePlasmaVirtualDesktop("desktop2");
    w->removePlasmaVirtualDesktop("desktop1");
    QVERIFY(dataChangedSpy.wait());
    QCOMPARE(dataChangedSpy.last().last().value<QVector<int>>(), QVector<int>{int(PlasmaWindowModel::IsOnAllDesktops)});
    QCOMPARE(model->data(index, PlasmaWindowModel::VirtualDesktops).toStringList(), QStringList({}));
    QCOMPARE(model->data(index, PlasmaWindowModel::IsOnAllDesktops).toBool(), true);

    QVERIFY(!dataChangedSpy.wait(100));
}

void PlasmaWindowModelTest::testRequests()
{
    // this test verifies that the various requests are properly passed to the server
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy rowInsertedSpy(model, &PlasmaWindowModel::rowsInserted);
    QVERIFY(rowInsertedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(rowInsertedSpy.wait());

    QSignalSpy activateRequestedSpy(w, &PlasmaWindowInterface::activeRequested);
    QVERIFY(activateRequestedSpy.isValid());
    QSignalSpy closeRequestedSpy(w, &PlasmaWindowInterface::closeRequested);
    QVERIFY(closeRequestedSpy.isValid());
    QSignalSpy moveRequestedSpy(w, &PlasmaWindowInterface::moveRequested);
    QVERIFY(moveRequestedSpy.isValid());
    QSignalSpy resizeRequestedSpy(w, &PlasmaWindowInterface::resizeRequested);
    QVERIFY(resizeRequestedSpy.isValid());
    QSignalSpy virtualDesktopRequestedSpy(w, &PlasmaWindowInterface::virtualDesktopRequested);
    QVERIFY(virtualDesktopRequestedSpy.isValid());
    QSignalSpy keepAboveRequestedSpy(w, &PlasmaWindowInterface::keepAboveRequested);
    QVERIFY(keepAboveRequestedSpy.isValid());
    QSignalSpy keepBelowRequestedSpy(w, &PlasmaWindowInterface::keepBelowRequested);
    QVERIFY(keepBelowRequestedSpy.isValid());
    QSignalSpy minimizedRequestedSpy(w, &PlasmaWindowInterface::minimizedRequested);
    QVERIFY(minimizedRequestedSpy.isValid());
    QSignalSpy maximizeRequestedSpy(w, &PlasmaWindowInterface::maximizedRequested);
    QVERIFY(maximizeRequestedSpy.isValid());
    QSignalSpy shadeRequestedSpy(w, &PlasmaWindowInterface::shadedRequested);
    QVERIFY(shadeRequestedSpy.isValid());

    // first let's use some invalid row numbers
    model->requestActivate(-1);
    model->requestClose(-1);
    model->requestVirtualDesktop(-1, 1);
    model->requestToggleKeepAbove(-1);
    model->requestToggleKeepBelow(-1);
    model->requestToggleMinimized(-1);
    model->requestToggleMaximized(-1);
    model->requestActivate(1);
    model->requestClose(1);
    model->requestMove(1);
    model->requestResize(1);
    model->requestVirtualDesktop(1, 1);
    model->requestToggleKeepAbove(1);
    model->requestToggleKeepBelow(1);
    model->requestToggleMinimized(1);
    model->requestToggleMaximized(1);
    model->requestToggleShaded(1);
    // that should not have triggered any signals
    QVERIFY(!activateRequestedSpy.wait(100));
    QVERIFY(activateRequestedSpy.isEmpty());
    QVERIFY(closeRequestedSpy.isEmpty());
    QVERIFY(moveRequestedSpy.isEmpty());
    QVERIFY(resizeRequestedSpy.isEmpty());
    QVERIFY(virtualDesktopRequestedSpy.isEmpty());
    QVERIFY(minimizedRequestedSpy.isEmpty());
    QVERIFY(maximizeRequestedSpy.isEmpty());
    QVERIFY(shadeRequestedSpy.isEmpty());

    // now with the proper row
    // activate
    model->requestActivate(0);
    QVERIFY(activateRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(activateRequestedSpy.first().first().toBool(), true);
    QCOMPARE(closeRequestedSpy.count(), 0);
    QCOMPARE(moveRequestedSpy.count(), 0);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // close
    model->requestClose(0);
    QVERIFY(closeRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 0);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // move
    model->requestMove(0);
    QVERIFY(moveRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 0);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // resize
    model->requestResize(0);
    QVERIFY(resizeRequestedSpy.wait());
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 0);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // virtual desktop
    model->requestVirtualDesktop(0, 1);
    QVERIFY(virtualDesktopRequestedSpy.wait());
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.first().first().toUInt(), 1u);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 0);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // keep above
    model->requestToggleKeepAbove(0);
    QVERIFY(keepAboveRequestedSpy.wait());
    QCOMPARE(keepAboveRequestedSpy.count(), 1);
    QCOMPARE(keepAboveRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // keep Below
    model->requestToggleKeepBelow(0);
    QVERIFY(keepBelowRequestedSpy.wait());
    QCOMPARE(keepBelowRequestedSpy.count(), 1);
    QCOMPARE(keepBelowRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // minimize
    model->requestToggleMinimized(0);
    QVERIFY(minimizedRequestedSpy.wait());
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 0);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // maximize
    model->requestToggleMaximized(0);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(shadeRequestedSpy.count(), 0);
    // shade
    model->requestToggleShaded(0);
    QVERIFY(shadeRequestedSpy.wait());
    QCOMPARE(shadeRequestedSpy.count(), 1);
    QCOMPARE(shadeRequestedSpy.first().first().toBool(), true);
    QCOMPARE(activateRequestedSpy.count(), 1);
    QCOMPARE(closeRequestedSpy.count(), 1);
    QCOMPARE(moveRequestedSpy.count(), 1);
    QCOMPARE(resizeRequestedSpy.count(), 1);
    QCOMPARE(virtualDesktopRequestedSpy.count(), 1);
    QCOMPARE(minimizedRequestedSpy.count(), 1);
    QCOMPARE(maximizeRequestedSpy.count(), 1);

    // the toggles can also support a different state
    QSignalSpy dataChangedSpy(model, &PlasmaWindowModel::dataChanged);
    QVERIFY(dataChangedSpy.isValid());
    // keepAbove
    w->setKeepAbove(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleKeepAbove(0);
    QVERIFY(keepAboveRequestedSpy.wait());
    QCOMPARE(keepAboveRequestedSpy.count(), 2);
    QCOMPARE(keepAboveRequestedSpy.last().first().toBool(), false);
    // keepBelow
    w->setKeepBelow(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleKeepBelow(0);
    QVERIFY(keepBelowRequestedSpy.wait());
    QCOMPARE(keepBelowRequestedSpy.count(), 2);
    QCOMPARE(keepBelowRequestedSpy.last().first().toBool(), false);
    // minimize
    w->setMinimized(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleMinimized(0);
    QVERIFY(minimizedRequestedSpy.wait());
    QCOMPARE(minimizedRequestedSpy.count(), 2);
    QCOMPARE(minimizedRequestedSpy.last().first().toBool(), false);
    // maximized
    w->setMaximized(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleMaximized(0);
    QVERIFY(maximizeRequestedSpy.wait());
    QCOMPARE(maximizeRequestedSpy.count(), 2);
    QCOMPARE(maximizeRequestedSpy.last().first().toBool(), false);
    // shaded
    w->setShaded(true);
    QVERIFY(dataChangedSpy.wait());
    model->requestToggleShaded(0);
    QVERIFY(shadeRequestedSpy.wait());
    QCOMPARE(shadeRequestedSpy.count(), 2);
    QCOMPARE(shadeRequestedSpy.last().first().toBool(), false);
}

void PlasmaWindowModelTest::testCreateWithUnmappedWindow()
{
    // this test verifies that creating the model just when an unmapped window exists doesn't cause problems
    // that is the unmapped window should be added (as expected), but also be removed again

    // create a window in "normal way"
    QSignalSpy windowCreatedSpy(m_pw, &PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(w);
    QVERIFY(windowCreatedSpy.wait());
    PlasmaWindow *window = windowCreatedSpy.first().first().value<PlasmaWindow*>();
    QVERIFY(window);
    // make sure the resource is properly created on server side
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);

    QSignalSpy unmappedSpy(window, &PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(window, &PlasmaWindow::destroyed);
    QVERIFY(destroyedSpy.isValid());
    // unmap should be triggered, but not yet the destroyed
    w->unmap();
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.isEmpty());

    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1);
    QSignalSpy rowRemovedSpy(model, &PlasmaWindowModel::rowsRemoved);
    QVERIFY(rowRemovedSpy.isValid());
    QVERIFY(rowRemovedSpy.wait());
    QCOMPARE(rowRemovedSpy.count(), 1);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(destroyedSpy.count(), 1);
}

void PlasmaWindowModelTest::testChangeWindowAfterModelDestroy_data()
{
    QTest::addColumn<ClientWindowSignal>("changedSignal");
    QTest::addColumn<QVariant>("setter");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("active")           << &PlasmaWindow::activeChanged                   << QVariant::fromValue(&PlasmaWindowInterface::setActive)                   << QVariant(true);
    QTest::newRow("minimized")        << &PlasmaWindow::minimizedChanged                << QVariant::fromValue(&PlasmaWindowInterface::setMinimized)                << QVariant(true);
    QTest::newRow("fullscreen")       << &PlasmaWindow::fullscreenChanged               << QVariant::fromValue(&PlasmaWindowInterface::setFullscreen)               << QVariant(true);
    QTest::newRow("keepAbove")        << &PlasmaWindow::keepAboveChanged                << QVariant::fromValue(&PlasmaWindowInterface::setKeepAbove)                << QVariant(true);
    QTest::newRow("keepBelow")        << &PlasmaWindow::keepBelowChanged                << QVariant::fromValue(&PlasmaWindowInterface::setKeepBelow)                << QVariant(true);
    QTest::newRow("maximized")        << &PlasmaWindow::maximizedChanged                << QVariant::fromValue(&PlasmaWindowInterface::setMaximized)                << QVariant(true);
    QTest::newRow("demandsAttention") << &PlasmaWindow::demandsAttentionChanged         << QVariant::fromValue(&PlasmaWindowInterface::setDemandsAttention)         << QVariant(true);
    QTest::newRow("closeable")        << &PlasmaWindow::closeableChanged                << QVariant::fromValue(&PlasmaWindowInterface::setCloseable)                << QVariant(true);
    QTest::newRow("minimizeable")     << &PlasmaWindow::minimizeableChanged             << QVariant::fromValue(&PlasmaWindowInterface::setMinimizeable)             << QVariant(true);
    QTest::newRow("maximizeable")     << &PlasmaWindow::maximizeableChanged             << QVariant::fromValue(&PlasmaWindowInterface::setMaximizeable)             << QVariant(true);
    QTest::newRow("fullscreenable")   << &PlasmaWindow::fullscreenableChanged           << QVariant::fromValue(&PlasmaWindowInterface::setFullscreenable)           << QVariant(true);
    QTest::newRow("skipTaskbar")      << &PlasmaWindow::skipTaskbarChanged              << QVariant::fromValue(&PlasmaWindowInterface::setSkipTaskbar)              << QVariant(true);
    QTest::newRow("shadeable")        << &PlasmaWindow::shadeableChanged                << QVariant::fromValue(&PlasmaWindowInterface::setShadeable)                << QVariant(true);
    QTest::newRow("shaded")           << &PlasmaWindow::shadedChanged                   << QVariant::fromValue(&PlasmaWindowInterface::setShaded)                   << QVariant(true);
    QTest::newRow("movable")          << &PlasmaWindow::movableChanged                  << QVariant::fromValue(&PlasmaWindowInterface::setMovable)                  << QVariant(true);
    QTest::newRow("resizable")        << &PlasmaWindow::resizableChanged                << QVariant::fromValue(&PlasmaWindowInterface::setResizable)                << QVariant(true);
    QTest::newRow("vdChangeable")     << &PlasmaWindow::virtualDesktopChangeableChanged << QVariant::fromValue(&PlasmaWindowInterface::setVirtualDesktopChangeable) << QVariant(true);
    QTest::newRow("onallDesktop")     << &PlasmaWindow::onAllDesktopsChanged            << QVariant::fromValue(&PlasmaWindowInterface::setOnAllDesktops)            << QVariant(true);
    QTest::newRow("title")            << &PlasmaWindow::titleChanged                    << QVariant::fromValue(&PlasmaWindowInterface::setTitle)                    << QVariant(QStringLiteral("foo"));
    QTest::newRow("appId")            << &PlasmaWindow::appIdChanged                    << QVariant::fromValue(&PlasmaWindowInterface::setAppId)                    << QVariant(QStringLiteral("foo"));
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 28)
    QTest::newRow("iconname" )            << &PlasmaWindow::iconChanged                     << QVariant::fromValue(&PlasmaWindowInterface::setThemedIconName)           << QVariant(QStringLiteral("foo"));
#endif
    QTest::newRow("icon" )            << &PlasmaWindow::iconChanged                     << QVariant::fromValue(&PlasmaWindowInterface::setIcon)                     << QVariant(QIcon::fromTheme(QStringLiteral("foo")));
    QTest::newRow("vd")               << &PlasmaWindow::virtualDesktopChanged           << QVariant::fromValue(&PlasmaWindowInterface::setVirtualDesktop)           << QVariant(2u);
    QTest::newRow("unmapped")         << &PlasmaWindow::unmapped                        << QVariant::fromValue(&PlasmaWindowInterface::unmap)                       << QVariant();
}

void PlasmaWindowModelTest::testChangeWindowAfterModelDestroy()
{
    // this test verifies that changes in a window after the model got destroyed doesn't crash
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    QSignalSpy windowCreatedSpy(m_pw, &PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());
    auto w = m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(windowCreatedSpy.wait());
    PlasmaWindow *window = windowCreatedSpy.first().first().value<PlasmaWindow*>();
    // make sure the resource is properly created on server side
    QCoreApplication::instance()->processEvents(QEventLoop::WaitForMoreEvents);
    QCOMPARE(model->rowCount(), 1);
    delete model;
    QFETCH(ClientWindowSignal, changedSignal);
    QSignalSpy changedSpy(window, changedSignal);
    QVERIFY(changedSpy.isValid());
    QVERIFY(!window->isActive());
    QFETCH(QVariant, setter);
    QFETCH(QVariant, value);
    if (QMetaType::Type(value.type()) == QMetaType::Bool) {
        (w->*(setter.value<ServerWindowBoolSetter>()))(value.toBool());
    } else if (QMetaType::Type(value.type()) == QMetaType::QString) {
        (w->*(setter.value<ServerWindowStringSetter>()))(value.toString());
    } else if (QMetaType::Type(value.type()) == QMetaType::UInt) {
        (w->*(setter.value<ServerWindowQuint32Setter>()))(value.toUInt());
    } else if (QMetaType::Type(value.type()) == QMetaType::QIcon) {
        (w->*(setter.value<ServerWindowIconSetter>()))(value.value<QIcon>());
    } else if (!value.isValid()) {
        (w->*(setter.value<ServerWindowVoidSetter>()))();
    }

    QVERIFY(changedSpy.wait());
}

void PlasmaWindowModelTest::testCreateWindowAfterModelDestroy()
{
    // this test verifies that creating a window after the model got destroyed doesn't crash
    auto model = m_pw->createWindowModel();
    QVERIFY(model);
    delete model;
    QSignalSpy windowCreatedSpy(m_pw, &PlasmaWindowManagement::windowCreated);
    QVERIFY(windowCreatedSpy.isValid());
    m_pwInterface->createWindow(m_pwInterface, QUuid::createUuid());
    QVERIFY(windowCreatedSpy.wait());
}

QTEST_GUILESS_MAIN(PlasmaWindowModelTest)
#include "test_plasma_window_model.moc"
