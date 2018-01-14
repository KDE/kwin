/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "platform.h"
#include "rules.h"
#include "screens.h"
#include "shell_client.h"
#include "virtualdesktops.h"
#include "wayland_server.h"

#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_shell_client_rules-0");


class TestShellClientRules : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testApplyInitialDesktop_data();
    void testApplyInitialDesktop();
    void testApplyInitialMinimize_data();
    void testApplyInitialMinimize();
    void testApplyInitialSkipTaskbar_data();
    void testApplyInitialSkipTaskbar();
    void testApplyInitialSkipPager_data();
    void testApplyInitialSkipPager();
    void testApplyInitialSkipSwitcher_data();
    void testApplyInitialSkipSwitcher();
    void testApplyInitialKeepAbove_data();
    void testApplyInitialKeepAbove();
    void testApplyInitialKeepBelow_data();
    void testApplyInitialKeepBelow();
    void testApplyInitialShortcut_data();
    void testApplyInitialShortcut();
};

void TestShellClientRules::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();

    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestShellClientRules::init()
{
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().first());
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
}

void TestShellClientRules::cleanup()
{
    Test::destroyWaylandConnection();
}

#define TEST_DATA( name ) \
void TestShellClientRules::name##_data() \
{ \
    QTest::addColumn<Test::ShellSurfaceType>("type"); \
    QTest::addColumn<int>("ruleNumber"); \
    QTest::newRow("wlShell|Force") << Test::ShellSurfaceType::WlShell << 2; \
    QTest::newRow("xdgShellV5|Force") << Test::ShellSurfaceType::XdgShellV5 << 2; \
    QTest::newRow("xdgShellV6|Force") << Test::ShellSurfaceType::XdgShellV6 << 2; \
    QTest::newRow("wlShell|Apply") << Test::ShellSurfaceType::WlShell << 3; \
    QTest::newRow("xdgShellV5|Apply") << Test::ShellSurfaceType::XdgShellV5 << 3; \
    QTest::newRow("xdgShellV6|Apply") << Test::ShellSurfaceType::XdgShellV6 << 3; \
    QTest::newRow("wlShell|ApplyNow") << Test::ShellSurfaceType::WlShell << 5; \
    QTest::newRow("xdgShellV5|ApplyNow") << Test::ShellSurfaceType::XdgShellV5 << 5; \
    QTest::newRow("xdgShellV6|ApplyNow") << Test::ShellSurfaceType::XdgShellV6 << 5; \
    QTest::newRow("wlShell|ForceTemporarily") << Test::ShellSurfaceType::WlShell << 6; \
    QTest::newRow("xdgShellV5|ForceTemporarily") << Test::ShellSurfaceType::XdgShellV5 << 6; \
    QTest::newRow("xdgShellV6|ForceTemporarily") << Test::ShellSurfaceType::XdgShellV6 << 6; \
}

TEST_DATA(testApplyInitialDesktop)

void TestShellClientRules::testApplyInitialDesktop()
{
    // ensure we have two desktops and are on first desktop
    VirtualDesktopManager::self()->setCount(2);
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().first());

    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("desktop=2\ndesktoprule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 2);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialMinimize)

void TestShellClientRules::testApplyInitialMinimize()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("minimize=1\nminimizerule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), true);
    QCOMPARE(c->isActive(), false);
    c->setMinimized(false);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialSkipTaskbar)

void TestShellClientRules::testApplyInitialSkipTaskbar()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("skiptaskbar=true\nskiptaskbarrule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), true);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialSkipPager)

void TestShellClientRules::testApplyInitialSkipPager()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("skippager=true\nskippagerrule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), true);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialSkipSwitcher)

void TestShellClientRules::testApplyInitialSkipSwitcher()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("skipswitcher=true\nskipswitcherrule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), true);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialKeepAbove)

void TestShellClientRules::testApplyInitialKeepAbove()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("above=true\naboverule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), true);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialKeepBelow)

void TestShellClientRules::testApplyInitialKeepBelow()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    QString rule = QStringLiteral("below=true\nbelowrule=%1").arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), true);
    QCOMPARE(c->shortcut(), QKeySequence());
}

TEST_DATA(testApplyInitialShortcut)

void TestShellClientRules::testApplyInitialShortcut()
{
    // install the temporary rule
    QFETCH(int, ruleNumber);
    const QKeySequence sequence{Qt::ControlModifier + Qt::ShiftModifier + Qt::MetaModifier + Qt::AltModifier + Qt::Key_Space};
    QString rule = QStringLiteral("shortcut=%1\nshortcutrule=%2").arg(sequence.toString()).arg(ruleNumber);
    QMetaObject::invokeMethod(RuleBook::self(), "temporaryRulesMessage", Q_ARG(QString, rule));

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(c->desktop(), 1);
    QCOMPARE(c->isMinimized(), false);
    QCOMPARE(c->isActive(), true);
    QCOMPARE(c->skipTaskbar(), false);
    QCOMPARE(c->skipPager(), false);
    QCOMPARE(c->skipSwitcher(), false);
    QCOMPARE(c->keepAbove(), false);
    QCOMPARE(c->keepBelow(), false);
    QCOMPARE(c->shortcut(), sequence);
}

WAYLANDTEST_MAIN(TestShellClientRules)
#include "shell_client_rules_test.moc"
