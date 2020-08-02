/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "x11client.h"
#include "composite.h"
#include "cursor.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "scene.h"
#include <kwineffects.h>

#include <KDecoration2/Decoration>

#include <QQuickItem>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_aurorae_destroy_deco-0");

class DontCrashAuroraeDestroyDecoTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testBorderlessMaximizedWindows();

};

void DontCrashAuroraeDestroyDecoTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("org.kde.kdecoration2").writeEntry("library", "org.kde.kwin.aurorae");
    config->sync();
    kwinApp()->setConfig(config);

    // this test needs to enforce OpenGL compositing to get into the crashy condition
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);
}

void DontCrashAuroraeDestroyDecoTest::init()
{
    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void DontCrashAuroraeDestroyDecoTest::testBorderlessMaximizedWindows()
{
    // this test verifies that Aurorae doesn't crash when clicking the maximize button
    // with kwin config option BorderlessMaximizedWindows
    // see BUG 362772

    // first adjust the config
    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // create an xcb window
    xcb_connection_t *c = xcb_connect(nullptr, nullptr);
    QVERIFY(!xcb_connection_has_error(c));

    xcb_window_t w = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, w, rootWindow(), 0, 0, 100, 200, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_map_window(c, w);
    xcb_flush(c);

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Client *client = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(client->isDecorated());
    QCOMPARE(client->maximizeMode(), MaximizeRestore);
    QCOMPARE(client->noBorder(), false);
    // verify that the deco is Aurorae
    QCOMPARE(qstrcmp(client->decoration()->metaObject()->className(), "Aurorae::Decoration"), 0);
    // find the maximize button
    QQuickItem *item = client->decoration()->findChild<QQuickItem*>("maximizeButton");
    QVERIFY(item);
    const QPointF scenePoint = item->mapToScene(QPoint(0, 0));

    // mark the window as ready for painting, otherwise it doesn't get input events
    QMetaObject::invokeMethod(client, "setReadyForPainting");
    QVERIFY(client->readyForPainting());

    // simulate click on maximize button
    QSignalSpy maximizedStateChangedSpy(client, static_cast<void (AbstractClient::*)(KWin::AbstractClient*, MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged));
    QVERIFY(maximizedStateChangedSpy.isValid());
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(client->frameGeometry().topLeft() + scenePoint.toPoint(), timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(maximizedStateChangedSpy.wait());
    QCOMPARE(client->maximizeMode(), MaximizeFull);
    QCOMPARE(client->noBorder(), true);

    // and destroy the window again
    xcb_unmap_window(c, w);
    xcb_destroy_window(c, w);
    xcb_flush(c);
    xcb_disconnect(c);

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::DontCrashAuroraeDestroyDecoTest)
#include "dont_crash_aurorae_destroy_deco.moc"
