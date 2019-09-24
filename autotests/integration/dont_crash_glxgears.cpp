/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "abstract_client.h"
#include "x11client.h"
#include "deleted.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

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
    qRegisterMetaType<KWin::Deleted*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void DontCrashGlxgearsTest::testGlxgears()
{
    // closing a glxgears window through Aurorae themes used to crash KWin
    // Let's make sure that doesn't happen anymore

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QProcess glxgears;
    glxgears.start(QStringLiteral("glxgears"));
    QVERIFY(glxgears.waitForStarted());

    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);
    QCOMPARE(workspace()->clientList().count(), 1);
    X11Client *glxgearsClient = workspace()->clientList().first();
    QVERIFY(glxgearsClient->isDecorated());
    QSignalSpy closedSpy(glxgearsClient, &X11Client::windowClosed);
    QVERIFY(closedSpy.isValid());
    KDecoration2::Decoration *decoration = glxgearsClient->decoration();
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
