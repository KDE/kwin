/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "platform.h"
#include "shell_client.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <KWayland/Client/surface.h>
#include <KWayland/Client/shell.h>

#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_keyboard_laout-0");

class KeyboardLayoutTest : public QObject
{
    Q_OBJECT
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

private:
    void reconfigureLayouts();
};

void KeyboardLayoutTest::reconfigureLayouts()
{
    // create DBus signal to reload
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/Layouts"), QStringLiteral("org.kde.keyboard"), QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);
}

void KeyboardLayoutTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    kwinApp()->setKxkbConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
}

void KeyboardLayoutTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void KeyboardLayoutTest::cleanup()
{
    Test::destroyWaylandConnection();
}

class LayoutChangedSignalWrapper : public QObject
{
    Q_OBJECT
public:
    LayoutChangedSignalWrapper()
        : QObject()
    {
        QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("currentLayoutChanged"), this, SIGNAL(layoutChanged(QString)));
    }

Q_SIGNALS:
    void layoutChanged(const QString &name);
};

void KeyboardLayoutTest::testReconfigure()
{
    // verifies that we can change the keymap

    // default should be a keymap with only us layout
    auto xkb = input()->keyboard()->xkb();
    QCOMPARE(xkb->numberOfLayouts(), 1u);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    auto layouts = xkb->layoutNames();
    QCOMPARE(layouts.size(), 1);
    QVERIFY(layouts.contains(0));
    QCOMPARE(layouts[0], QStringLiteral("English (US)"));

    // create a new keymap
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("de,us"));
    layoutGroup.sync();

    reconfigureLayouts();
    // now we should have two layouts
    QTRY_COMPARE(xkb->numberOfLayouts(), 2u);
    // default layout is German
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    layouts = xkb->layoutNames();
    QCOMPARE(layouts.size(), 2);
    QVERIFY(layouts.contains(0));
    QVERIFY(layouts.contains(1));
    QCOMPARE(layouts[0], QStringLiteral("German"));
    QCOMPARE(layouts[1], QStringLiteral("English (US)"));
}

void KeyboardLayoutTest::testChangeLayoutThroughDBus()
{
    // this test verifies that the layout can be changed through DBus
    // first configure layouts
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("de,us,de(neo)"));
    layoutGroup.sync();
    reconfigureLayouts();
    // now we should have two layouts
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 3u);
    // default layout is German
    xkb->switchToLayout(0);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));

    LayoutChangedSignalWrapper wrapper;
    QSignalSpy layoutChangedSpy(&wrapper, &LayoutChangedSignalWrapper::layoutChanged);
    QVERIFY(layoutChangedSpy.isValid());

    // now change through DBus to english
    auto changeLayout = [] (const QString &layoutName) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("setLayout"));
        msg << layoutName;
        return QDBusConnection::sessionBus().asyncCall(msg);
    };
    auto reply = changeLayout(QStringLiteral("English (US)"));
    reply.waitForFinished();
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    layoutChangedSpy.clear();

    // switch to a layout which does not exist
    reply = changeLayout(QStringLiteral("French"));
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), false);
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    QVERIFY(!layoutChangedSpy.wait());
    QVERIFY(layoutChangedSpy.isEmpty());

    // switch to another layout should work
    reply = changeLayout(QStringLiteral("German"));
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    layoutChangedSpy.clear();

    // switching to same layout should also work
    reply = changeLayout(QStringLiteral("German"));
    QVERIFY(!reply.isError());
    QCOMPARE(reply.reply().arguments().first().toBool(), true);
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    QVERIFY(!layoutChangedSpy.wait());
    QVERIFY(layoutChangedSpy.isEmpty());
}

void KeyboardLayoutTest::testPerLayoutShortcut()
{
    // this test verifies that per-layout global shortcuts are working correctly.
    // first configure layouts
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.sync();

    // and create the global shortcuts
    const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("Switch keyboard layout to English (US)"));
    a->setProperty("componentName", componentName);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{Qt::CTRL+Qt::ALT+Qt::Key_1}, KGlobalAccel::NoAutoloading);
    delete a;
    a = new QAction(this);
    a->setObjectName(QStringLiteral("Switch keyboard layout to German"));
    a->setProperty("componentName", componentName);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{Qt::CTRL+Qt::ALT+Qt::Key_2}, KGlobalAccel::NoAutoloading);
    delete a;

    reconfigureLayouts();
    // now we should have three layouts
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 3u);
    // default layout is English
    xkb->switchToLayout(0);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    LayoutChangedSignalWrapper wrapper;
    QSignalSpy layoutChangedSpy(&wrapper, &LayoutChangedSignalWrapper::layoutChanged);
    QVERIFY(layoutChangedSpy.isValid());

    // now switch to English through the global shortcut
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    QVERIFY(layoutChangedSpy.wait());
    // now layout should be German
    QCOMPARE(xkb->layoutName(), QStringLiteral("German"));
    // release keys again
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    // switch back to English
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // release keys again
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
}

void KeyboardLayoutTest::testDBusServiceExport()
{
    // verifies that the dbus service is only exported if there are at least two layouts

    // first configure layouts, with just one layout
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 1u);
    // default layout is English
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // with one layout we should not have the dbus interface
    QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());

    // reconfigure to two layouts
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de"));
    layoutGroup.sync();
    reconfigureLayouts();
    QTRY_COMPARE(xkb->numberOfLayouts(), 2u);
    QTRY_VERIFY(QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());

    // and back to one layout
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us"));
    layoutGroup.sync();
    reconfigureLayouts();
    QTRY_COMPARE(xkb->numberOfLayouts(), 1u);
    QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.keyboard")).value());
}

void KeyboardLayoutTest::testVirtualDesktopPolicy()
{
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("Desktop"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 3u);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    VirtualDesktopManager::self()->setCount(4);
    QCOMPARE(VirtualDesktopManager::self()->count(), 4u);

    auto changeLayout = [] (const QString &layoutName) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("setLayout"));
        msg << layoutName;
        return QDBusConnection::sessionBus().asyncCall(msg);
    };
    auto reply = changeLayout(QStringLiteral("German"));
    reply.waitForFinished();
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));

    // switch to another virtual desktop
    auto desktops = VirtualDesktopManager::self()->desktops();
    QCOMPARE(desktops.count(), 4);
    QCOMPARE(desktops.first(), VirtualDesktopManager::self()->currentDesktop());
    VirtualDesktopManager::self()->setCurrent(desktops.at(1));
    QCOMPARE(desktops.at(1), VirtualDesktopManager::self()->currentDesktop());
    // should be reset to English
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));reply = changeLayout(QStringLiteral("German (Neo 2)"));
    reply.waitForFinished();
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // back to desktop 0 -> German
    VirtualDesktopManager::self()->setCurrent(desktops.at(0));
    QCOMPARE(desktops.first(), VirtualDesktopManager::self()->currentDesktop());
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));
    // desktop 2 -> English
    VirtualDesktopManager::self()->setCurrent(desktops.at(2));
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // desktop 1 -> Neo
    VirtualDesktopManager::self()->setCurrent(desktops.at(1));
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // remove virtual desktops
    VirtualDesktopManager::self()->setCount(1);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));

    // add another desktop
    VirtualDesktopManager::self()->setCount(2);
    // switching to it should result in going to default
    desktops = VirtualDesktopManager::self()->desktops();
    QCOMPARE(desktops.count(), 2);
    QCOMPARE(desktops.first(), VirtualDesktopManager::self()->currentDesktop());
    VirtualDesktopManager::self()->setCurrent(desktops.last());
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

}

void KeyboardLayoutTest::testWindowPolicy()
{
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("Window"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 3u);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // create a window
    using namespace KWayland::Client;
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto c1 = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c1);

    // now switch layout
    auto changeLayout = [] (const QString &layoutName) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("setLayout"));
        msg << layoutName;
        return QDBusConnection::sessionBus().asyncCall(msg);
    };
    auto reply = changeLayout(QStringLiteral("German"));
    reply.waitForFinished();
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));

    // create a second window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface2(Test::createShellSurface(surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 100), Qt::red);
    QVERIFY(c2);
    // this should have switched back to English
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));
    // now change to another layout
    reply = changeLayout(QStringLiteral("German (Neo 2)"));
    reply.waitForFinished();
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // activate other window
    workspace()->activateClient(c1);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));
    workspace()->activateClient(c2);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));
}

void KeyboardLayoutTest::testApplicationPolicy()
{
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("us,de,de(neo)"));
    layoutGroup.writeEntry("SwitchMode", QStringLiteral("WinClass"));
    layoutGroup.sync();
    reconfigureLayouts();
    auto xkb = input()->keyboard()->xkb();
    QTRY_COMPARE(xkb->numberOfLayouts(), 3u);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("English (US)"));

    // create a window
    using namespace KWayland::Client;
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto c1 = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c1);

    // now switch layout
    auto changeLayout = [] (const QString &layoutName) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), QStringLiteral("setLayout"));
        msg << layoutName;
        return QDBusConnection::sessionBus().asyncCall(msg);
    };
    LayoutChangedSignalWrapper wrapper;
    QSignalSpy layoutChangedSpy(&wrapper, &LayoutChangedSignalWrapper::layoutChanged);
    QVERIFY(layoutChangedSpy.isValid());
    auto reply = changeLayout(QStringLiteral("German"));
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    reply.waitForFinished();
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German"));

    // create a second window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface2(Test::createShellSurface(surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 100), Qt::red);
    QVERIFY(c2);
    // it is the same application and should not switch the layout
    QVERIFY(!layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 1);
    // now change to another layout
    reply = changeLayout(QStringLiteral("German (Neo 2)"));
    QVERIFY(layoutChangedSpy.wait());
    QCOMPARE(layoutChangedSpy.count(), 2);
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    // activate other window
    workspace()->activateClient(c1);
    QVERIFY(!layoutChangedSpy.wait());
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));
    workspace()->activateClient(c2);
    QVERIFY(!layoutChangedSpy.wait());
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));

    shellSurface2.reset();
    surface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(c2));
    QVERIFY(!layoutChangedSpy.wait());
    QTRY_COMPARE(xkb->layoutName(), QStringLiteral("German (Neo 2)"));
}

WAYLANDTEST_MAIN(KeyboardLayoutTest)
#include "keyboard_layout_test.moc"
