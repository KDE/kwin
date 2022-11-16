/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "deleted.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"

#include <KDecoration2/Decoration>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_glxgears-0");

class DontCrashGlxgearsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testGlxgears();
};

void DontCrashGlxgearsTest::initTestCase()
{
    qRegisterMetaType<KWin::Deleted *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void DontCrashGlxgearsTest::testGlxgears()
{
    // closing a glxgears window through Aurorae themes used to crash KWin
    // Let's make sure that doesn't happen anymore

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    QProcess glxgears;
    glxgears.setProgram(QStringLiteral("glxgears"));
    glxgears.start();
    QVERIFY(glxgears.waitForStarted());

    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(windowAddedSpy.count(), 1);
    QCOMPARE(workspace()->clientList().count(), 1);
    X11Window *glxgearsWindow = workspace()->clientList().first();
    QVERIFY(glxgearsWindow->isDecorated());
    QSignalSpy closedSpy(glxgearsWindow, &X11Window::windowClosed);
    KDecoration2::Decoration *decoration = glxgearsWindow->decoration();
    QVERIFY(decoration);

    // send a mouse event to the position of the close button
    // TODO: position is dependent on the decoration in use. We should use a static target instead, a fake deco for autotests.
    QPointF pos = decoration->rect().topRight() + QPointF(-decoration->borderTop() / 2, decoration->borderTop() / 2);
    QHoverEvent event(QEvent::HoverMove, pos, pos);
    QCoreApplication::instance()->sendEvent(decoration, &event);
    // mouse press
    QMouseEvent mousePressevent(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mousePressevent.setAccepted(false);
    QCoreApplication::sendEvent(decoration, &mousePressevent);
    QVERIFY(mousePressevent.isAccepted());
    // mouse Release
    QMouseEvent mouseReleaseEvent(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mouseReleaseEvent.setAccepted(false);
    QCoreApplication::sendEvent(decoration, &mouseReleaseEvent);
    QVERIFY(mouseReleaseEvent.isAccepted());

    QVERIFY(closedSpy.wait());
    QCOMPARE(closedSpy.count(), 1);
    xcb_flush(connection());

    if (glxgears.state() == QProcess::Running) {
        QVERIFY(glxgears.waitForFinished());
    }
}

}

WAYLANDTEST_MAIN(KWin::DontCrashGlxgearsTest)
#include "dont_crash_glxgears.moc"
