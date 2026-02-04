/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 KWin Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "scripting/scripting.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/seat.h>

#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_scripting_input_events-0");

class TestProbe : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void keyPressed()
    {
        m_keyPresses++;
    }
    Q_INVOKABLE void keyReleased()
    {
        m_keyReleases++;
    }
    Q_INVOKABLE void mouseMoved()
    {
        m_mouseMoves++;
    }
    Q_INVOKABLE void mousePressed()
    {
        m_mousePresses++;
    }
    Q_INVOKABLE void mouseReleased()
    {
        m_mouseReleases++;
    }

    int keyPresses() const
    {
        return m_keyPresses;
    }
    int keyReleases() const
    {
        return m_keyReleases;
    }
    int mouseMoves() const
    {
        return m_mouseMoves;
    }
    int mousePresses() const
    {
        return m_mousePresses;
    }
    int mouseReleases() const
    {
        return m_mouseReleases;
    }

private:
    int m_keyPresses = 0;
    int m_keyReleases = 0;
    int m_mouseMoves = 0;
    int m_mousePresses = 0;
    int m_mouseReleases = 0;
};

class ScriptingInputEventsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSignals();
};

void ScriptingInputEventsTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));

    // empty config to have defaults
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    Test::setOutputConfig({Rect(0, 0, 1280, 1024)});
    QVERIFY(Scripting::self());
}

void ScriptingInputEventsTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    input()->pointer()->warp(QPointF(640, 512));
}

void ScriptingInputEventsTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ScriptingInputEventsTest::testSignals()
{
    // Load QML declarative script that hooks InputEvents and calls into TestProbe.
    const QString qmlPath = QFINDTESTDATA("./scripts/inputevents.qml");
    QVERIFY(!qmlPath.isEmpty());

    auto probe = std::make_unique<TestProbe>();
    Scripting::self()->declarativeScriptSharedContext()->setContextProperty(QStringLiteral("TestProbe"), probe.get());

    const QString pluginName = QStringLiteral("test.inputevents");
    QVERIFY(!Scripting::self()->isScriptLoaded(pluginName));
    const int id = Scripting::self()->loadDeclarativeScript(qmlPath, pluginName);
    QVERIFY(id != -1);
    QTRY_VERIFY(Scripting::self()->isScriptLoaded(pluginName));
    auto script = Scripting::self()->findScript(pluginName);
    QVERIFY(script);
    QSignalSpy runningChangedSpy(script, &AbstractScript::runningChanged);
    script->run();
    QTRY_COMPARE(runningChangedSpy.count(), 1);

    quint32 timestamp = 0;

    // keyboard press / release
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_VERIFY(probe->keyPresses() >= 1);
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_VERIFY(probe->keyReleases() >= 1);

    // mouse move
    Test::pointerMotion(QPointF(100, 100), timestamp++);
    QTRY_VERIFY(probe->mouseMoves() >= 1);

    // mouse press / release
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_VERIFY(probe->mousePresses() >= 1);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QTRY_VERIFY(probe->mouseReleases() >= 1);
}

WAYLANDTEST_MAIN(ScriptingInputEventsTest)
#include "input_events_test.moc"
