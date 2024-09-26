/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "effect/effectloader.h"
#include "pointer_input.h"
#include "scripting/scripting.h"
#include "wayland_server.h"
#include "workspace.h"

#define private public
#include "screenedge.h"
#undef private

#include <KConfigGroup>

Q_DECLARE_METATYPE(KWin::ElectricBorder)

using namespace std::chrono_literals;
using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_scripting_screenedge-0");

class ScreenEdgeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testEdge_data();
    void testEdge();
    void testTouchEdge_data();
    void testTouchEdge();
    void testEdgeUnregister();
    void testDeclarativeTouchEdge();

private:
    void triggerConfigReload();
};

void ScreenEdgeTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

    // empty config to have defaults
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);

    // disable all effects to prevent them grabbing edges
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    // disable electric border pushback
    config->group(QStringLiteral("Windows")).writeEntry("ElectricBorderPushbackPixels", 0);
    config->group(QStringLiteral("TabBox")).writeEntry("TouchBorderActivate", int(ElectricNone));

    config->sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(Scripting::self());

    workspace()->screenEdges()->setTimeThreshold(0ms);
    workspace()->screenEdges()->setReActivationThreshold(0ms);
}

void ScreenEdgeTest::init()
{
    KWin::input()->pointer()->warp(QPointF(640, 512));
    if (workspace()->showingDesktop()) {
        workspace()->slotToggleShowDesktop();
    }
    QVERIFY(!workspace()->showingDesktop());
}

void ScreenEdgeTest::cleanup()
{
    // try to unload the script
    const QStringList scripts = {QFINDTESTDATA("./scripts/screenedge.js"), QFINDTESTDATA("./scripts/screenedgeunregister.js"), QFINDTESTDATA("./scripts/touchScreenedge.js")};
    for (const QString &script : scripts) {
        if (!script.isEmpty()) {
            if (Scripting::self()->isScriptLoaded(script)) {
                QVERIFY(Scripting::self()->unloadScript(script));
                QTRY_VERIFY(!Scripting::self()->isScriptLoaded(script));
            }
        }
    }
}

void ScreenEdgeTest::testEdge_data()
{
    QTest::addColumn<KWin::ElectricBorder>("edge");
    QTest::addColumn<QPoint>("triggerPos");

    QTest::newRow("Top") << KWin::ElectricTop << QPoint(512, 0);
    QTest::newRow("TopRight") << KWin::ElectricTopRight << QPoint(1279, 0);
    QTest::newRow("Right") << KWin::ElectricRight << QPoint(1279, 512);
    QTest::newRow("BottomRight") << KWin::ElectricBottomRight << QPoint(1279, 1023);
    QTest::newRow("Bottom") << KWin::ElectricBottom << QPoint(512, 1023);
    QTest::newRow("BottomLeft") << KWin::ElectricBottomLeft << QPoint(0, 1023);
    QTest::newRow("Left") << KWin::ElectricLeft << QPoint(0, 512);
    QTest::newRow("TopLeft") << KWin::ElectricTopLeft << QPoint(0, 0);

    // repeat a row to show previously unloading and re-registering works
    QTest::newRow("Top") << KWin::ElectricTop << QPoint(512, 0);
}

void ScreenEdgeTest::testEdge()
{
    const QString scriptToLoad = QFINDTESTDATA("./scripts/screenedge.js");
    QVERIFY(!scriptToLoad.isEmpty());

    // mock the config
    auto config = kwinApp()->config();
    QFETCH(KWin::ElectricBorder, edge);
    config->group(QLatin1String("Script-") + scriptToLoad).writeEntry("Edge", int(edge));
    config->sync();

    QVERIFY(!Scripting::self()->isScriptLoaded(scriptToLoad));
    const int id = Scripting::self()->loadScript(scriptToLoad);
    QVERIFY(id != -1);
    QVERIFY(Scripting::self()->isScriptLoaded(scriptToLoad));
    auto s = Scripting::self()->findScript(scriptToLoad);
    QVERIFY(s);
    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    s->run();
    QVERIFY(runningChangedSpy.wait());
    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(runningChangedSpy.first().first().toBool(), true);
    // triggering the edge will result in show desktop being triggered
    QSignalSpy showDesktopSpy(workspace(), &Workspace::showingDesktopChanged);

    // trigger the edge
    QFETCH(QPoint, triggerPos);
    KWin::input()->pointer()->warp(triggerPos);
    QCOMPARE(showDesktopSpy.count(), 1);
    QVERIFY(workspace()->showingDesktop());
}

void ScreenEdgeTest::testTouchEdge_data()
{
    QTest::addColumn<KWin::ElectricBorder>("edge");
    QTest::addColumn<QPoint>("triggerPos");
    QTest::addColumn<QPoint>("motionPos");

    QTest::newRow("Top") << KWin::ElectricTop << QPoint(50, 0) << QPoint(50, 500);
    QTest::newRow("Right") << KWin::ElectricRight << QPoint(1279, 50) << QPoint(500, 50);
    QTest::newRow("Bottom") << KWin::ElectricBottom << QPoint(512, 1023) << QPoint(512, 500);
    QTest::newRow("Left") << KWin::ElectricLeft << QPoint(0, 50) << QPoint(500, 50);

    // repeat a row to show previously unloading and re-registering works
    QTest::newRow("Top") << KWin::ElectricTop << QPoint(512, 0) << QPoint(512, 500);
}

void ScreenEdgeTest::testTouchEdge()
{
    const QString scriptToLoad = QFINDTESTDATA("./scripts/touchScreenedge.js");
    QVERIFY(!scriptToLoad.isEmpty());

    // mock the config
    auto config = kwinApp()->config();
    QFETCH(KWin::ElectricBorder, edge);
    config->group(QLatin1String("Script-") + scriptToLoad).writeEntry("Edge", int(edge));
    config->sync();

    QVERIFY(!Scripting::self()->isScriptLoaded(scriptToLoad));
    const int id = Scripting::self()->loadScript(scriptToLoad);
    QVERIFY(id != -1);
    QVERIFY(Scripting::self()->isScriptLoaded(scriptToLoad));
    auto s = Scripting::self()->findScript(scriptToLoad);
    QVERIFY(s);
    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    s->run();
    QVERIFY(runningChangedSpy.wait());
    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(runningChangedSpy.first().first().toBool(), true);
    // triggering the edge will result in show desktop being triggered
    QSignalSpy showDesktopSpy(workspace(), &Workspace::showingDesktopChanged);

    // trigger the edge
    QFETCH(QPoint, triggerPos);
    quint32 timestamp = 0;
    Test::touchDown(0, triggerPos, timestamp++);
    QFETCH(QPoint, motionPos);
    Test::touchMotion(0, motionPos, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(showDesktopSpy.wait());
    QCOMPARE(showDesktopSpy.count(), 1);
    QVERIFY(workspace()->showingDesktop());
}

void ScreenEdgeTest::triggerConfigReload()
{
    workspace()->slotReconfigure();
}

void ScreenEdgeTest::testEdgeUnregister()
{
    const QString scriptToLoad = QFINDTESTDATA("./scripts/screenedgeunregister.js");
    QVERIFY(!scriptToLoad.isEmpty());

    Scripting::self()->loadScript(scriptToLoad);
    auto s = Scripting::self()->findScript(scriptToLoad);
    auto configGroup = s->config();
    configGroup.writeEntry("Edge", int(KWin::ElectricLeft));
    configGroup.sync();
    const QPoint triggerPos = QPoint(0, 512);

    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    s->run();
    QVERIFY(runningChangedSpy.wait());

    QSignalSpy showDesktopSpy(workspace(), &Workspace::showingDesktopChanged);

    // trigger the edge
    KWin::input()->pointer()->warp(triggerPos);
    QCOMPARE(showDesktopSpy.count(), 1);

    // reset
    KWin::input()->pointer()->warp(QPointF(500, 500));
    workspace()->slotToggleShowDesktop();
    showDesktopSpy.clear();

    // trigger again, to show that retriggering works
    KWin::input()->pointer()->warp(triggerPos);
    QCOMPARE(showDesktopSpy.count(), 1);

    // reset
    KWin::input()->pointer()->warp(QPointF(500, 500));
    workspace()->slotToggleShowDesktop();
    showDesktopSpy.clear();

    // make the script unregister the edge
    configGroup.writeEntry("mode", "unregister");
    triggerConfigReload();
    KWin::input()->pointer()->warp(triggerPos);
    QCOMPARE(showDesktopSpy.count(), 0); // not triggered

    // force the script to unregister a non-registered edge to prove it doesn't explode
    triggerConfigReload();
}

void ScreenEdgeTest::testDeclarativeTouchEdge()
{
    const QString scriptToLoad = QFINDTESTDATA("./scripts/screenedgetouch.qml");
    QVERIFY(!scriptToLoad.isEmpty());
    QVERIFY(Scripting::self()->loadDeclarativeScript(scriptToLoad) != -1);
    QVERIFY(Scripting::self()->isScriptLoaded(scriptToLoad));

    auto s = Scripting::self()->findScript(scriptToLoad);
    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    s->run();
    QTRY_COMPARE(runningChangedSpy.count(), 1);

    QSignalSpy showDesktopSpy(workspace(), &Workspace::showingDesktopChanged);

    // Trigger the edge through touch
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(0, 50), timestamp++);
    Test::touchMotion(0, QPointF(500, 50), timestamp++);
    Test::touchUp(0, timestamp++);

    QVERIFY(showDesktopSpy.wait());
}

WAYLANDTEST_MAIN(ScreenEdgeTest)
#include "screenedge_test.moc"
