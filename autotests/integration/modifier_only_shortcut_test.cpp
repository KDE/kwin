/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>

#include <QDBusConnection>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_modifier_only_shortcut-0");
static const QString s_serviceName = QStringLiteral("org.kde.KWin.Test.ModifierOnlyShortcut");
static const QString s_path = QStringLiteral("/Test");

class ModifierOnlyShortcutTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testTrigger_data();
    void testTrigger();
    void testCapsLock();
    void testGlobalShortcutsDisabled_data();
    void testGlobalShortcutsDisabled();
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

void ModifierOnlyShortcutTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void ModifierOnlyShortcutTest::init()
{
    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void ModifierOnlyShortcutTest::cleanup()
{
}

void ModifierOnlyShortcutTest::testTrigger_data()
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

void ModifierOnlyShortcutTest::testTrigger()
{
    // this test verifies that modifier only shortcut triggers correctly
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
    QCOMPARE(triggeredSpy.count(), 1);

    // the other shortcuts should not trigger
    QFETCH(QList<int>, nonTriggeringMods);
    for (auto it = nonTriggeringMods.constBegin(), end = nonTriggeringMods.constEnd(); it != end; it++) {
        kwinApp()->platform()->keyboardKeyPressed(*it, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(*it, timestamp++);
        QCOMPARE(triggeredSpy.count(), 1);
    }

    // try configured again
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // click another key while modifier is held
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // release other key after modifier release
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // press key before pressing modifier
    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // mouse button pressed before clicking modifier
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::LeftButton);
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::NoButton);
    QCOMPARE(triggeredSpy.count(), 2);

    // mouse button press before mod press, release before mod release
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::LeftButton);
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::NoButton);
    QCOMPARE(triggeredSpy.count(), 2);

    // mouse button click while mod is pressed
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::LeftButton);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(input()->qtButtonStates(), Qt::NoButton);
    QCOMPARE(triggeredSpy.count(), 2);

    // scroll while mod is pressed
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // same for horizontal
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // now try to lock the screen while modifier key is pressed
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    QVERIFY(Test::lockScreen());
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    // now trigger while screen is locked, should also not work
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 2);

    QVERIFY(Test::unlockScreen());
}

void ModifierOnlyShortcutTest::testCapsLock()
{
    // this test verifies that Capslock does not trigger the shift shortcut
    // but other shortcuts still trigger even when Capslock is on
    Target target;
    QSignalSpy triggeredSpy(&target, &Target::shortcutTriggered);
    QVERIFY(triggeredSpy.isValid());

    KConfigGroup group = kwinApp()->config()->group("ModifierOnlyShortcuts");
    group.writeEntry("Meta", QStringList());
    group.writeEntry("Alt", QStringList());
    group.writeEntry("Shift", QStringList{s_serviceName, s_path, s_serviceName, QStringLiteral("shortcut")});
    group.writeEntry("Control", QStringList());
    group.sync();
    workspace()->slotReconfigure();

    // first test that the normal shortcut triggers
    quint32 timestamp = 1;
    const int modifier = KEY_LEFTSHIFT;
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 1);

    // now capslock
    kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier);
    QCOMPARE(triggeredSpy.count(), 1);

    // currently caps lock is on
    // shift still triggers
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier);
    QCOMPARE(triggeredSpy.count(), 2);

    // meta should also trigger
    group.writeEntry("Meta", QStringList{s_serviceName, s_path, s_serviceName, QStringLiteral("shortcut")});
    group.writeEntry("Alt", QStringList());
    group.writeEntry("Shift", QStringList{});
    group.writeEntry("Control", QStringList());
    group.sync();
    workspace()->slotReconfigure();
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier | Qt::MetaModifier);
    QCOMPARE(input()->keyboard()->xkb()->modifiersRelevantForGlobalShortcuts(), Qt::MetaModifier);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QCOMPARE(triggeredSpy.count(), 3);

    // set back to shift to ensure we don't trigger with capslock
    group.writeEntry("Meta", QStringList());
    group.writeEntry("Alt", QStringList());
    group.writeEntry("Shift", QStringList{s_serviceName, s_path, s_serviceName, QStringLiteral("shortcut")});
    group.writeEntry("Control", QStringList());
    group.sync();
    workspace()->slotReconfigure();

    // release caps lock
    kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);
    QCOMPARE(triggeredSpy.count(), 3);
}

void ModifierOnlyShortcutTest::testGlobalShortcutsDisabled_data()
{
    QTest::addColumn<QStringList>("metaConfig");
    QTest::addColumn<QStringList>("altConfig");
    QTest::addColumn<QStringList>("controlConfig");
    QTest::addColumn<QStringList>("shiftConfig");
    QTest::addColumn<int>("modifier");

    const QStringList trigger = QStringList{s_serviceName, s_path, s_serviceName, QStringLiteral("shortcut")};
    const QStringList e = QStringList();

    QTest::newRow("leftMeta") << trigger << e << e << e << KEY_LEFTMETA;
    QTest::newRow("rightMeta") << trigger << e << e << e << KEY_RIGHTMETA;
    QTest::newRow("leftAlt") << e << trigger << e << e << KEY_LEFTALT;
    QTest::newRow("rightAlt") << e << trigger << e << e << KEY_RIGHTALT;
    QTest::newRow("leftControl") << e << e << trigger << e << KEY_LEFTCTRL;
    QTest::newRow("rightControl") << e << e << trigger << e << KEY_RIGHTCTRL;
    QTest::newRow("leftShift") << e << e << e << trigger << KEY_LEFTSHIFT;
    QTest::newRow("rightShift") << e << e <<  e << trigger <<KEY_RIGHTSHIFT;
}

void ModifierOnlyShortcutTest::testGlobalShortcutsDisabled()
{
    // this test verifies that when global shortcuts are disabled inside KWin (e.g. through a window rule)
    // the modifier only shortcuts do not trigger.
    // see BUG: 370146
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

    // trigger once to verify the shortcut works
    quint32 timestamp = 1;
    QFETCH(int, modifier);
    QVERIFY(!workspace()->globalShortcutsDisabled());
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 1);
    triggeredSpy.clear();

    // now disable global shortcuts
    workspace()->disableGlobalShortcutsForClient(true);
    QVERIFY(workspace()->globalShortcutsDisabled());
    // Should not get triggered
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 0);
    triggeredSpy.clear();

    // enable again
    workspace()->disableGlobalShortcutsForClient(false);
    QVERIFY(!workspace()->globalShortcutsDisabled());
    // should get triggered again
    kwinApp()->platform()->keyboardKeyPressed(modifier, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(modifier, timestamp++);
    QCOMPARE(triggeredSpy.count(), 1);
}

WAYLANDTEST_MAIN(ModifierOnlyShortcutTest)
#include "modifier_only_shortcut_test.moc"
