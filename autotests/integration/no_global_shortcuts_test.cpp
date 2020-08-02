/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "platform.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QDBusConnection>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

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
    emit shortcutTriggered();
}

void NoGlobalShortcutsTest::initTestCase()
{
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit(), KWin::WaylandServer::InitializationFlag::NoGlobalShortcuts));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void NoGlobalShortcutsTest::init()
{
    screens()->setCurrent(0);
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
    QTest::newRow("rightShift") << e << e <<  e << trigger <<KEY_RIGHTSHIFT << QList<int>{KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTMETA, KEY_RIGHTMETA};
}

void NoGlobalShortcutsTest::testTrigger()
{
    // test based on ModifierOnlyShortcutTest::testTrigger
    Target target;
    QSignalSpy triggeredSpy(&target, &Target::shortcutTriggered);
    QVERIFY(triggeredSpy.isValid());

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
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 0);

    // the other shortcuts should not trigger
    QFETCH(QList<int>, nonTriggeringMods);
    for (auto it = nonTriggeringMods.constBegin(), end = nonTriggeringMods.constEnd(); it != end; it++) {
        kwinApp()->platform()->keyboardKeyPressed(*it, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(*it, timestamp++);
        QCOMPARE(triggeredSpy.count(), 0);
    }
}

void NoGlobalShortcutsTest::testKGlobalAccel()
{
    QScopedPointer<QAction> action(new QAction(nullptr));
    action->setProperty("componentName", QStringLiteral(KWIN_NAME));
    action->setObjectName(QStringLiteral("globalshortcuts-test-meta-shift-w"));
    QSignalSpy triggeredSpy(action.data(), &QAction::triggered);
    QVERIFY(triggeredSpy.isValid());
    KGlobalAccel::self()->setShortcut(action.data(), QList<QKeySequence>{Qt::META + Qt::SHIFT + Qt::Key_W}, KGlobalAccel::NoAutoloading);
    input()->registerShortcut(Qt::META + Qt::SHIFT + Qt::Key_W, action.data());

    // press meta+shift+w
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::MetaModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier | Qt::MetaModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_W, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_W, timestamp++);

    // release meta+shift
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QVERIFY(!triggeredSpy.wait());
    QCOMPARE(triggeredSpy.count(), 0);
}

void NoGlobalShortcutsTest::testPointerShortcut()
{
    // based on LockScreenTest::testPointerShortcut
    QScopedPointer<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.data(), &QAction::triggered);
    QVERIFY(actionSpy.isValid());
    input()->registerPointerShortcut(Qt::MetaModifier, Qt::LeftButton, action.data());

    // try to trigger the shortcut
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
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
    QScopedPointer<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.data(), &QAction::triggered);
    QVERIFY(actionSpy.isValid());
    QFETCH(Qt::Orientation, direction);
    QFETCH(int, sign);
    PointerAxisDirection axisDirection = PointerAxisUp;
    if (direction == Qt::Vertical) {
        axisDirection = sign > 0 ? PointerAxisUp : PointerAxisDown;
    } else {
        axisDirection = sign > 0 ? PointerAxisLeft : PointerAxisRight;
    }
    input()->registerAxisShortcut(Qt::MetaModifier, axisDirection, action.data());

    // try to trigger the shortcut
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    if (direction == Qt::Vertical)
        kwinApp()->platform()->pointerAxisVertical(sign * 5.0, timestamp++);
    else
        kwinApp()->platform()->pointerAxisHorizontal(sign * 5.0, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QCoreApplication::instance()->processEvents();
    QCOMPARE(actionSpy.count(), 0);
}

void NoGlobalShortcutsTest::testScreenEdge()
{
    // based on LockScreenTest::testScreenEdge
    QSignalSpy screenEdgeSpy(ScreenEdges::self(), &ScreenEdges::approaching);
    QVERIFY(screenEdgeSpy.isValid());
    QCOMPARE(screenEdgeSpy.count(), 0);

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion({5, 5}, timestamp++);
    QCOMPARE(screenEdgeSpy.count(), 0);
}

WAYLANDTEST_MAIN(NoGlobalShortcutsTest)
#include "no_global_shortcuts_test.moc"
