/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "screenedge.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QAction>
#include <QDBusConnection>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_no_global_shortcuts-0");
static const QString s_serviceName = QStringLiteral("org.kde.KWin.Test.ModifierOnlyShortcut");
static const QString s_path = QStringLiteral("/Test");

Q_DECLARE_METATYPE(KWin::ElectricBorder)

/**
 * This test verifies the NoGlobalShortcuts initialization flag
 */
class NoGlobalShortcutsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testTrigger_data();
    void testTrigger();
    void testKGlobalAccel();
    void testPointerShortcut();
    void testAxisShortcut_data();
    void testAxisShortcut();
    void testScreenEdge();
};

class Target : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.Test.ModifierOnlyShortcut")

public:
    Target();
    ~Target() override;

public Q_SLOTS:
    Q_SCRIPTABLE void shortcut();

Q_SIGNALS:
    void shortcutTriggered();
};

Target::Target()
    : QObject()
{
    QDBusConnection::sessionBus().registerService(s_serviceName);
    QDBusConnection::sessionBus().registerObject(s_path, s_serviceName, this, QDBusConnection::ExportScriptableSlots);
}

Target::~Target()
{
    QDBusConnection::sessionBus().unregisterObject(s_path);
    QDBusConnection::sessionBus().unregisterService(s_serviceName);
}

void Target::shortcut()
{
    Q_EMIT shortcutTriggered();
}

void NoGlobalShortcutsTest::initTestCase()
{
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName, KWin::WaylandServer::InitializationFlag::NoGlobalShortcuts));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void NoGlobalShortcutsTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void NoGlobalShortcutsTest::cleanup()
{
}

void NoGlobalShortcutsTest::testTrigger_data()
{
    QTest::addColumn<QStringList>("metaConfig");
    QTest::addColumn<QStringList>("altConfig");
    QTest::addColumn<QStringList>("controlConfig");
    QTest::addColumn<QStringList>("shiftConfig");
    QTest::addColumn<int>("modifier");
    QTest::addColumn<QList<int>>("nonTriggeringMods");

    const QStringList trigger = QStringList{s_serviceName, s_path, s_serviceName, QStringLiteral("shortcut")};
    const QStringList e = QStringList();

    QTest::newRow("leftMeta") << trigger << e << e << e << KEY_LEFTMETA << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("rightMeta") << trigger << e << e << e << KEY_RIGHTMETA << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("leftAlt") << e << trigger << e << e << KEY_LEFTALT << QList<int>{KEY_LEFTMETA, KEY_RIGHTMETA, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("rightAlt") << e << trigger << e << e << KEY_RIGHTALT << QList<int>{KEY_LEFTMETA, KEY_RIGHTMETA, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("leftControl") << e << e << trigger << e << KEY_LEFTCTRL << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("rightControl") << e << e << trigger << e << KEY_RIGHTCTRL << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};
    QTest::newRow("leftShift") << e << e << e << trigger << KEY_LEFTSHIFT << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTMETA, KEY_RIGHTMETA};
    QTest::newRow("rightShift") << e << e << e << trigger << KEY_RIGHTSHIFT << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTMETA, KEY_RIGHTMETA};
}

void NoGlobalShortcutsTest::testTrigger()
{
    // test based on ModifierOnlyShortcutTest::testTrigger
    Target target;
    QSignalSpy triggeredSpy(&target, &Target::shortcutTriggered);

    KConfigGroup group = kwinApp()->config()->group("ModifierOnlyShortcuts");
    QFETCH(QStringList, metaConfig);
    QFETCH(QStringList, altConfig);
    QFETCH(QStringList, shiftConfig);
    QFETCH(QStringList, controlConfig);
    group.writeEntry("Meta", metaConfig);
    group.writeEntry("Alt", altConfig);
    group.writeEntry("Shift", shiftConfig);
    group.writeEntry("Control", controlConfig);
    group.sync();
    workspace()->slotReconfigure();

    // configured shortcut should trigger
    quint32 timestamp = 1;
    QFETCH(int, modifier);
    Test::keyboardKeyPressed(modifier, timestamp++);
    Test::keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 0);

    // the other shortcuts should not trigger
    QFETCH(QList<int>, nonTriggeringMods);
    for (auto it = nonTriggeringMods.constBegin(), end = nonTriggeringMods.constEnd(); it != end; it++) {
        Test::keyboardKeyPressed(*it, timestamp++);
        Test::keyboardKeyReleased(*it, timestamp++);
        QCOMPARE(triggeredSpy.count(), 0);
    }
}

void NoGlobalShortcutsTest::testKGlobalAccel()
{
    std::unique_ptr<QAction> action(new QAction(nullptr));
    action->setProperty("componentName", QStringLiteral("kwin"));
    action->setObjectName(QStringLiteral("globalshortcuts-test-meta-shift-w"));
    QSignalSpy triggeredSpy(action.get(), &QAction::triggered);
    KGlobalAccel::self()->setShortcut(action.get(), QList<QKeySequence>{Qt::META | Qt::SHIFT | Qt::Key_W}, KGlobalAccel::NoAutoloading);

    // press meta+shift+w
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::MetaModifier);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier | Qt::MetaModifier);
    Test::keyboardKeyPressed(KEY_W, timestamp++);
    Test::keyboardKeyReleased(KEY_W, timestamp++);

    // release meta+shift
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QVERIFY(!triggeredSpy.wait(100));
    QCOMPARE(triggeredSpy.count(), 0);
}

void NoGlobalShortcutsTest::testPointerShortcut()
{
    // based on LockScreenTest::testPointerShortcut
    std::unique_ptr<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.get(), &QAction::triggered);
    input()->registerPointerShortcut(Qt::MetaModifier, Qt::LeftButton, action.get());

    // try to trigger the shortcut
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
}

void NoGlobalShortcutsTest::testAxisShortcut_data()
{
    QTest::addColumn<Qt::Orientation>("direction");
    QTest::addColumn<int>("sign");

    QTest::newRow("up") << Qt::Vertical << 1;
    QTest::newRow("down") << Qt::Vertical << -1;
    QTest::newRow("left") << Qt::Horizontal << 1;
    QTest::newRow("right") << Qt::Horizontal << -1;
}

void NoGlobalShortcutsTest::testAxisShortcut()
{
    // based on LockScreenTest::testAxisShortcut
    std::unique_ptr<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.get(), &QAction::triggered);
    QFETCH(Qt::Orientation, direction);
    QFETCH(int, sign);
    PointerAxisDirection axisDirection = PointerAxisUp;
    if (direction == Qt::Vertical) {
        axisDirection = sign > 0 ? PointerAxisUp : PointerAxisDown;
    } else {
        axisDirection = sign > 0 ? PointerAxisLeft : PointerAxisRight;
    }
    input()->registerAxisShortcut(Qt::MetaModifier, axisDirection, action.get());

    // try to trigger the shortcut
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    if (direction == Qt::Vertical) {
        Test::pointerAxisVertical(sign * 5.0, timestamp++);
    } else {
        Test::pointerAxisHorizontal(sign * 5.0, timestamp++);
    }
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
}

void NoGlobalShortcutsTest::testScreenEdge()
{
    // based on LockScreenTest::testScreenEdge
    QSignalSpy screenEdgeSpy(workspace()->screenEdges(), &ScreenEdges::approaching);
    QCOMPARE(screenEdgeSpy.count(), 0);

    quint32 timestamp = 1;
    Test::pointerMotion({5, 5}, timestamp++);
    QCOMPARE(screenEdgeSpy.count(), 0);
}

WAYLANDTEST_MAIN(NoGlobalShortcutsTest)
#include "no_global_shortcuts_test.moc"
