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
#include "wayland_server.h"

#include <KConfigGroup>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

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
}

void KeyboardLayoutTest::cleanup()
{
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

WAYLANDTEST_MAIN(KeyboardLayoutTest)
#include "keyboard_layout_test.moc"
