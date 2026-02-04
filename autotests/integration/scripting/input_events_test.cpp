/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 KWin Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "scripting/inputeventscripting.h"
#include "scripting/scripting.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/seat.h>

#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_scripting_input_events-0");

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
    auto *spyObject = Scripting::self()->inputEventSpy();
    QVERIFY(spyObject);

    QSignalSpy keyPressedSpy(spyObject, &InputEventScriptingSpy::keyPressed);
    QSignalSpy keyReleasedSpy(spyObject, &InputEventScriptingSpy::keyReleased);
    QSignalSpy mouseMovedSpy(spyObject, &InputEventScriptingSpy::mouseMoved);
    QSignalSpy mousePressedSpy(spyObject, &InputEventScriptingSpy::mousePressed);
    QSignalSpy mouseReleasedSpy(spyObject, &InputEventScriptingSpy::mouseReleased);

    quint32 timestamp = 0;

    // keyboard press / release
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(keyPressedSpy.wait());
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keyReleasedSpy.wait());

    // mouse move
    Test::pointerMotion(QPointF(100, 100), timestamp++);
    QVERIFY(mouseMovedSpy.wait());

    // mouse press / release
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(mousePressedSpy.wait());
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(mouseReleasedSpy.wait());
}

WAYLANDTEST_MAIN(ScriptingInputEventsTest)
#include "input_events_test.moc"
