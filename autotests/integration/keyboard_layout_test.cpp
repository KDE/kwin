/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <KWayland/Client/surface.h>

#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_keyboard_laout-0");

class KeyboardLayoutTest : public QObject
{
    Q_OBJECT
public:
    KeyboardLayoutTest()
        : layoutsReconfiguredSpy(this, &KeyboardLayoutTest::layoutListChanged)
        , layoutChangedSpy(this, &KeyboardLayoutTest::layoutChanged)
    {

        QVERIFY(QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("layoutListChanged"), this, SIGNAL(layoutListChanged())));
        QVERIFY(QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("layoutChanged"), this, SIGNAL(layoutChanged(uint))));
    }

Q_SIGNALS:
    void layoutChanged(uint index);
    void layoutListChanged();

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testReconfigure();
    void testChangeLayoutThroughDBus();
    void testPerLayoutShortcut();
    void testDBusServiceExport();
    void testVirtualDesktopPolicy();
    void testWindowPolicy();
    void testApplicationPolicy();
    void testNumLock();

private:
    void reconfigureLayouts();
    void resetLayouts();
    auto changeLayout(uint index);
    void callSession(const QString &method);
    QSignalSpy layoutsReconfiguredSpy;
    QSignalSpy layoutChangedSpy;
    KConfigGroup layoutGroup;
};

void KeyboardLayoutTest::reconfigureLayouts()
{
    // create DBus signal to reload
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/Layouts"), QStringLiteral("org.kde.keyboard"), QStringLiteral("reloadConfig"));
    QVERIFY(QDBusConnection::sessionBus().send(message));

    QVERIFY(layoutsReconfiguredSpy.wait(1000));
    QCOMPARE(layoutsReconfiguredSpy.count(), 1);
    layoutsReconfiguredSpy.clear();
}

void KeyboardLayoutTest::resetLayouts()
{
    /* Switch Policy to destroy layouts from memory.
     * On return to original Policy they should reload from disk.
     */
    callSession(QStringLiteral("aboutToSaveSession"));

    const QString policy = layoutGroup.readEntry("SwitchMode", "Global");

    if (policy == QLatin1String("Global")) {
        layoutGroup.writeEntry("SwitchMode", "Desktop");
    } else {
        layoutGroup.deleteEntry("SwitchMode");
    }
    reconfigureLayouts();

    layoutGroup.writeEntry("SwitchMode", policy);
    reconfigureLayouts();

    callSession(QStringLiteral("loadSession"));
}

auto KeyboardLayoutTest::changeLayout(uint index)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"),
                                                      QStringLiteral("/Layouts"),
                                                      QStringLiteral("org.kde.KeyboardLayouts"),
                                                      QStringLiteral("setLayout"));
    msg << index;
    return QDBusConnection::sessionBus().asyncCall(msg);
}

void KeyboardLayoutTest::callSession(const QString &method)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/Session"),
                                                      QStringLiteral("org.kde.KWin.Session"),
                                                      method);
    msg << QLatin1String(); // session name
    QVERIFY(QDBusConnection::sessionBus().call(msg).type() != QDBusMessage::ErrorMessage);
}

void KeyboardLayoutTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    kwinApp()->setKxkbConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.deleteGroup();

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());

    // don't get DBus signal on one-layout configuration
    //    QVERIFY(layoutsReconfiguredSpy.wait());
    //    QCOMPARE(layoutsReconfiguredSpy.count(), 1);
    //    layoutsReconfiguredSpy.clear();
}

void KeyboardLayoutTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void KeyboardLayoutTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void KeyboardLayoutTest::testReconfigure()
{
    // verifies that we can change the keymap

    // default should be a keymap with only us layout
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 1u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    QCOMPARE(xkb->numberOfLayouts(), 1);
    QCOMPARE(xkb->layoutName(0), QStringLiteral("English (US)"));

    // create a new keymap
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("de,us"));
    layoutGroup.sync();

    reconfigureLayouts();
    // now we should have two layouts
    QCOMPARE(xkb->numberOfLayouts(), 2u);
    // default layout is German
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    QCOMPARE(xkb->numberOfLayouts(), 2);
    QCOMPARE(xkb->layoutName(0), QStringLiteral("German"));
    QCOMPARE(xkb->layoutName(1), QStringLiteral("English (US)"));
}

void KeyboardLayoutTest::testChangeLayoutThroughDBus()
{
    // this test verifies that the layout can be changed through DBus
    // first configure layouts
    enum Layout {
        de,
        us,
        de_neo,
        bad,
    };
    layoutGroup.writeEntry("LayoutList", QStringLiteral("de,us,de(neo)"));
    layoutGroup.sync();
    reconfigureLayouts();
    // now we should have three layouts
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 3u);
    // default layout is German
    xkb->switchToLayout(0);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));

    // place garbage to layout entry
    layoutGroup.writeEntry("LayoutDefaultFoo", "garbage");
    // make sure the garbage is wiped out on saving
    resetLayouts();
    QVERIFY(!layoutGroup.hasKey("LayoutDefaultFoo"));

    // now change through DBus to English
    auto reply = changeLayout(Layout::us);
    reply.waitForFinished();
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    layoutChangedSpy.clear();

    // layout should persist after reset
    resetLayouts();
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    layoutChangedSpy.clear();

    // switch to a layout which does not exist
    reply = changeLayout(Layout::bad);
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), false);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    QVERIFY(!layoutChangedSpy.wait(1000));

    // switch to another layout should work
    reply = changeLayout(Layout::de);
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    QVERIFY(layoutChangedSpy.wait(1000));
    QCOMPARE(layoutChangedSpy.count(), 1);

    // switching to same layout should also work
    reply = changeLayout(Layout::de);
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    QVERIFY(!layoutChangedSpy.wait(1000));
}

void KeyboardLayoutTest::testPerLayoutShortcut()
{
    // this test verifies that per-layout global shortcuts are working correctly.
    // first configure layouts
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.sync();

    // and create the global shortcuts
    const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("Switch keyboard layout to English (US)"));
    a->setProperty("componentName", componentName);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{Qt::CTRL | Qt::ALT | Qt::Key_1}, KGlobalAccel::NoAutoloading);
    delete a;
    a = new QAction(this);
    a->setObjectName(QStringLiteral("Switch keyboard layout to German"));
    a->setProperty("componentName", componentName);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{Qt::CTRL | Qt::ALT | Qt::Key_2}, KGlobalAccel::NoAutoloading);
    delete a;

    // now we should have three layouts
    auto xkb = input()->keyboard()->xkb();
    reconfigureLayouts();
    QCOMPARE(xkb->numberOfLayouts(), 3u);
    // default layout is English
    xkb->switchToLayout(0);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // now switch to English through the global shortcut
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    QVERIFY(layoutChangedSpy.wait());
    // now layout should be German
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    // release keys again
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    // switch back to English
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // release keys again
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
}

void KeyboardLayoutTest::testDBusServiceExport()
{
    // verifies that the dbus service is only exported if there are at least two layouts

    // first configure layouts, with just one layout
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 1u);
    // default layout is English
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // with one layout we should not have the dbus interface
    QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());

    // reconfigure to two layouts
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de"));
    layoutGroup.sync();
    reconfigureLayouts();
    QCOMPARE(xkb->numberOfLayouts(), 2u);
    QVERIFY(QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());

    // and back to one layout
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us"));
    layoutGroup.sync();
    reconfigureLayouts();
    QCOMPARE(xkb->numberOfLayouts(), 1u);
    QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());
}

void KeyboardLayoutTest::testVirtualDesktopPolicy()
{
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("Desktop"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 3u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    VirtualDesktopManager::self()->setCount(4);
    QCOMPARE(VirtualDesktopManager::self()->count(), 4u);
    auto desktops = VirtualDesktopManager::self()->desktops();
    QCOMPARE(desktops.count(), 4);

    // give desktops different layouts
    uint desktop, layout;
    for (desktop = 0; desktop < VirtualDesktopManager::self()->count(); ++desktop) {
        // switch to another virtual desktop
        VirtualDesktopManager::self()->setCurrent(desktops.at(desktop));
        QCOMPARE(desktops.at(desktop), VirtualDesktopManager::self()->currentDesktop());
        // should be reset to English
        QCOMPARE(xkb->currentLayout(), 0);
        // change first desktop to German
        layout = (desktop + 1) % xkb->numberOfLayouts();
        changeLayout(layout).waitForFinished();
        QCOMPARE(xkb->currentLayout(), layout);
    }

    // imitate app restart to test layouts saving feature
    resetLayouts();

    // check layout set on desktop switching as intended
    for (--desktop;;) {
        QCOMPARE(desktops.at(desktop), VirtualDesktopManager::self()->currentDesktop());
        layout = (desktop + 1) % xkb->numberOfLayouts();
        QCOMPARE(xkb->currentLayout(), layout);
        if (--desktop >= VirtualDesktopManager::self()->count()) { // overflow
            break;
        }
        VirtualDesktopManager::self()->setCurrent(desktops.at(desktop));
    }

    // remove virtual desktops
    desktop = 0;
    const KWin::VirtualDesktop *deletedDesktop = desktops.last();
    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(xkb->currentLayout(), layout = (desktop + 1) % xkb->numberOfLayouts());
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));

    // add another desktop
    VirtualDesktopManager::self()->setCount(2);
    // switching to it should result in going to default
    desktops = VirtualDesktopManager::self()->desktops();
    QCOMPARE(desktops.count(), 2);
    QCOMPARE(desktops.first(), VirtualDesktopManager::self()->currentDesktop());
    VirtualDesktopManager::self()->setCurrent(desktops.last());
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // check there are no more layouts left in config than the last actual non-default layouts number
    QSignalSpy deletedDesktopSpy(deletedDesktop, &VirtualDesktop::aboutToBeDestroyed);
    QVERIFY(deletedDesktopSpy.wait());
    resetLayouts();
    QCOMPARE(layoutGroup.keyList().filter(QStringLiteral("LayoutDefault")).count(), 1);
}

void KeyboardLayoutTest::testWindowPolicy()
{
    enum Layout {
        us,
        de,
        de_neo,
        bad,
    };
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("Window"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 3u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto c1 = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(c1);

    // now switch layout
    auto reply = changeLayout(Layout::de);
    reply.waitForFinished();
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));

    // create a second window
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 100), Qt::red);
    QVERIFY(c2);
    // this should have switched back to English
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // now change to another layout
    reply = changeLayout(Layout::de_neo);
    reply.waitForFinished();
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // activate other window
    workspace()->activateWindow(c1);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    workspace()->activateWindow(c2);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));
}

void KeyboardLayoutTest::testApplicationPolicy()
{
    enum Layout {
        us,
        de,
        de_neo,
        bad,
    };
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("WinClass"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 3u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    auto c1 = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(c1);

    // create a second window
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    shellSurface2->set_app_id(QStringLiteral("org.kde.foo"));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 100), Qt::red);
    QVERIFY(c2);
    // now switch layout
    layoutChangedSpy.clear();
    changeLayout(Layout::de_neo);
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    layoutChangedSpy.clear();
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    resetLayouts();
    // to trigger layout application for current client
    workspace()->activateWindow(c1);
    workspace()->activateWindow(c2);
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // activate other window
    workspace()->activateWindow(c1);
    // it is the same application and should not switch the layout
    QVERIFY(!layoutChangedSpy.wait(1000));
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));
    workspace()->activateWindow(c2);
    QVERIFY(!layoutChangedSpy.wait(1000));
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    shellSurface2.reset();
    surface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(c2));
    QVERIFY(!layoutChangedSpy.wait(1000));
    QCOMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    resetLayouts();
    QCOMPARE(layoutGroup.keyList().filter(QStringLiteral("LayoutDefault")).count(), 1);
}

void KeyboardLayoutTest::testNumLock()
{
    qputenv("KWIN_FORCE_NUM_LOCK_EVALUATION", "1");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us"));
    layoutGroup.sync();
    reconfigureLayouts();

    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 1u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // by default not set
    QVERIFY(!xkb->leds().testFlag(LED::NumLock));
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_NUMLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_NUMLOCK, timestamp++);
    // now it should be on
    QVERIFY(xkb->leds().testFlag(LED::NumLock));
    // and back to off
    Test::keyboardKeyPressed(KEY_NUMLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_NUMLOCK, timestamp++);
    QVERIFY(!xkb->leds().testFlag(LED::NumLock));

    // let's reconfigure to enable through config
    auto group = InputConfig::self()->inputConfig()->group("Keyboard");
    group.writeEntry("NumLock", 0);
    group.sync();
    xkb->reconfigure();
    // now it should be on
    QVERIFY(xkb->leds().testFlag(LED::NumLock));
    // pressing should result in it being off
    Test::keyboardKeyPressed(KEY_NUMLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_NUMLOCK, timestamp++);
    QVERIFY(!xkb->leds().testFlag(LED::NumLock));

    // pressing again should enable it
    Test::keyboardKeyPressed(KEY_NUMLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_NUMLOCK, timestamp++);
    QVERIFY(xkb->leds().testFlag(LED::NumLock));

    // now reconfigure to disable on load
    group.writeEntry("NumLock", 1);
    group.sync();
    xkb->reconfigure();
    QVERIFY(!xkb->leds().testFlag(LED::NumLock));
}

WAYLANDTEST_MAIN(KeyboardLayoutTest)
#include "keyboard_layout_test.moc"
