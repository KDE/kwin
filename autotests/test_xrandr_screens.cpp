/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "../plugins/platforms/x11/standalone/screens_xrandr.h"
#include "../cursor.h"
#include "../xcbutils.h"
#include "mock_workspace.h"
// Qt
#include <QtTest>
// system
#include <unistd.h>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

// mocking
namespace KWin
{

QPoint Cursor::pos()
{
    return QPoint(0, 0);
}
} // namespace KWin

static xcb_window_t s_rootWindow = XCB_WINDOW_NONE;
static xcb_connection_t *s_connection = nullptr;

using namespace KWin;
using namespace KWin::Xcb;

class TestXRandRScreens : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testStartup();
    void testChange();
    void testMultipleChanges();
private:
    QScopedPointer<QProcess> m_xserver;
};

void TestXRandRScreens::initTestCase()
{
    // TODO: turn into init instead of initTestCase
    // needs to be initTestCase as KWin::connection caches the first created xcb_connection_t
    // thus changing X server for each test run would create problems
    qsrand(QDateTime::currentMSecsSinceEpoch());
    // first reset just to be sure
    s_connection = nullptr;
    s_rootWindow = XCB_WINDOW_NONE;
    // start X Server
    m_xserver.reset(new QProcess);
    // use pipe to pass fd to Xephyr to get back the display id
    int pipeFds[2];
    QVERIFY(pipe(pipeFds) == 0);
    // using Xephyr as Xvfb doesn't support render extension
    m_xserver->start(QStringLiteral("Xephyr"), QStringList({ QStringLiteral("-displayfd"), QString::number(pipeFds[1]) }));
    QVERIFY(m_xserver->waitForStarted());
    QCOMPARE(m_xserver->state(), QProcess::Running);

    // reads from pipe, closes write side
    close(pipeFds[1]);

    QFile readPipe;
    QVERIFY(readPipe.open(pipeFds[0], QIODevice::ReadOnly, QFileDevice::AutoCloseHandle));
    QByteArray displayNumber = readPipe.readLine();
    readPipe.close();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() -1, 1);

    // create X connection
    int screen = 0;
    s_connection = xcb_connect(displayNumber.constData(), &screen);
    QVERIFY(s_connection);

    // set root window
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(s_connection));
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(s_connection));
            it.rem;
            --screen, xcb_screen_next(&it)) {
        if (screen == 0) {
            s_rootWindow = iter.data->root;
            break;
        }
    }
    QVERIFY(s_rootWindow != XCB_WINDOW_NONE);
    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(s_rootWindow));
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(s_connection));

    // get the extensions
    if (!Extensions::self()->isRandrAvailable()) {
        QSKIP("XRandR extension required");
    }
    for (const auto &extension : Extensions::self()->extensions()) {
        if (extension.name == QByteArrayLiteral("RANDR")) {
            if (extension.version < 1 * 0x10 + 4) {
                QSKIP("At least XRandR 1.4 required");
            }
        }
    }
}

void TestXRandRScreens::cleanupTestCase()
{
    Extensions::destroy();
    // close connection
    xcb_disconnect(s_connection);
    s_connection = nullptr;
    s_rootWindow = XCB_WINDOW_NONE;
    // kill X
    m_xserver->terminate();
    m_xserver->waitForFinished();
}

void TestXRandRScreens::testStartup()
{
    KWin::MockWorkspace ws;
    QScopedPointer<XRandRScreens> screens(new XRandRScreens(this));
    QVERIFY(!screens->eventTypes().isEmpty());
    QCOMPARE(screens->eventTypes().first(), Xcb::Extensions::self()->randrNotifyEvent());
    QCOMPARE(screens->extension(), 0);
    QCOMPARE(screens->genericEventTypes(), QVector<int>{0});
    screens->init();
    QRect xephyrDefault = QRect(0, 0, 640, 480);
    QCOMPARE(screens->count(), 1);
    QCOMPARE(screens->geometry(0), xephyrDefault);
    QCOMPARE(screens->geometry(1), QRect());
    QCOMPARE(screens->geometry(-1), QRect());
    QCOMPARE(static_cast<Screens*>(screens.data())->geometry(), xephyrDefault);
    QCOMPARE(screens->size(0), xephyrDefault.size());
    QCOMPARE(screens->size(1), QSize());
    QCOMPARE(screens->size(-1), QSize());
    QCOMPARE(static_cast<Screens*>(screens.data())->size(), xephyrDefault.size());
    // unfortunately we only have one output, so let's try at least to test somewhat
    QCOMPARE(screens->number(QPoint(0, 0)), 0);
    QCOMPARE(screens->number(QPoint(639, 479)), 0);
    QCOMPARE(screens->number(QPoint(1280, 1024)), 0);

    // let's change the mode
    RandR::CurrentResources resources(s_rootWindow);
    auto *crtcs = resources.crtcs();
    auto *modes = xcb_randr_get_screen_resources_current_modes(resources.data());
    auto *outputs = xcb_randr_get_screen_resources_current_outputs(resources.data());
    RandR::SetCrtcConfig setter(crtcs[0], resources->timestamp, resources->config_timestamp, 0, 0, modes[0].id, XCB_RANDR_ROTATION_ROTATE_0, 1, outputs);
    QVERIFY(!setter.isNull());

    // now let's recreate the XRandRScreens
    screens.reset(new XRandRScreens(this));
    screens->init();
    QRect geo = QRect(0, 0, modes[0].width, modes[0].height);
    QCOMPARE(screens->count(), 1);
    QCOMPARE(screens->geometry(0), geo);
    QCOMPARE(static_cast<Screens*>(screens.data())->geometry(), geo);
    QCOMPARE(screens->size(0), geo.size());
    QCOMPARE(static_cast<Screens*>(screens.data())->size(), geo.size());
}

void TestXRandRScreens::testChange()
{
    KWin::MockWorkspace ws;
    QScopedPointer<XRandRScreens> screens(new XRandRScreens(this));
    screens->init();

    // create some signal spys
    QSignalSpy changedSpy(screens.data(), SIGNAL(changed()));
    QVERIFY(changedSpy.isValid());
    QVERIFY(changedSpy.isEmpty());
    QVERIFY(changedSpy.wait());
    changedSpy.clear();
    QSignalSpy geometrySpy(screens.data(), SIGNAL(geometryChanged()));
    QVERIFY(geometrySpy.isValid());
    QVERIFY(geometrySpy.isEmpty());
    QSignalSpy sizeSpy(screens.data(), SIGNAL(sizeChanged()));
    QVERIFY(sizeSpy.isValid());
    QVERIFY(sizeSpy.isEmpty());

    // clear the event loop
    while (xcb_generic_event_t *e = xcb_poll_for_event(s_connection)) {
        free(e);
    }

    // let's change
    RandR::CurrentResources resources(s_rootWindow);
    auto *crtcs = resources.crtcs();
    auto *modes = xcb_randr_get_screen_resources_current_modes(resources.data());
    auto *outputs = xcb_randr_get_screen_resources_current_outputs(resources.data());
    RandR::SetCrtcConfig setter(crtcs[0], resources->timestamp, resources->config_timestamp, 0, 0, modes[1].id, XCB_RANDR_ROTATION_ROTATE_0, 1, outputs);
    xcb_flush(s_connection);
    QVERIFY(!setter.isNull());
    QVERIFY(setter->status == XCB_RANDR_SET_CONFIG_SUCCESS);

    xcb_generic_event_t *e = xcb_wait_for_event(s_connection);
    screens->event(e);
    free(e);

    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(sizeSpy.size(), 1);
    QCOMPARE(geometrySpy.size(), 1);
    QRect geo = QRect(0, 0, modes[1].width, modes[1].height);
    QCOMPARE(screens->count(), 1);
    QCOMPARE(screens->geometry(0), geo);
    QCOMPARE(static_cast<Screens*>(screens.data())->geometry(), geo);
    QCOMPARE(screens->size(0), geo.size());
    QCOMPARE(static_cast<Screens*>(screens.data())->size(), geo.size());
}

void TestXRandRScreens::testMultipleChanges()
{
    KWin::MockWorkspace ws;
    // multiple changes should only hit one changed signal
    QScopedPointer<XRandRScreens> screens(new XRandRScreens(this));
    screens->init();

    // create some signal spys
    QSignalSpy changedSpy(screens.data(), SIGNAL(changed()));
    QVERIFY(changedSpy.isValid());
    QVERIFY(changedSpy.isEmpty());
    QVERIFY(changedSpy.wait());
    changedSpy.clear();
    QSignalSpy geometrySpy(screens.data(), SIGNAL(geometryChanged()));
    QVERIFY(geometrySpy.isValid());
    QVERIFY(geometrySpy.isEmpty());
    QSignalSpy sizeSpy(screens.data(), SIGNAL(sizeChanged()));
    QVERIFY(sizeSpy.isValid());
    QVERIFY(sizeSpy.isEmpty());

    // clear the event loop
    while (xcb_generic_event_t *e = xcb_poll_for_event(s_connection)) {
        free(e);
    }

    // first change
    RandR::CurrentResources resources(s_rootWindow);
    auto *crtcs = resources.crtcs();
    auto *modes = xcb_randr_get_screen_resources_current_modes(resources.data());
    auto *outputs = xcb_randr_get_screen_resources_current_outputs(resources.data());
    RandR::SetCrtcConfig setter(crtcs[0], resources->timestamp, resources->config_timestamp, 0, 0, modes[0].id, XCB_RANDR_ROTATION_ROTATE_0, 1, outputs);
    QVERIFY(!setter.isNull());
    QVERIFY(setter->status == XCB_RANDR_SET_CONFIG_SUCCESS);
    // second change
    RandR::SetCrtcConfig setter2(crtcs[0], setter->timestamp, resources->config_timestamp, 0, 0, modes[1].id, XCB_RANDR_ROTATION_ROTATE_0, 1, outputs);
    QVERIFY(!setter2.isNull());
    QVERIFY(setter2->status == XCB_RANDR_SET_CONFIG_SUCCESS);

    auto passEvent = [&screens]() {
        xcb_generic_event_t *e = xcb_wait_for_event(s_connection);
        screens->event(e);
        free(e);
    };
    passEvent();
    passEvent();

    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.size(), 1);
    // previous state was modes[1] so the size didn't change
    QVERIFY(sizeSpy.isEmpty());
    QVERIFY(geometrySpy.isEmpty());
    QRect geo = QRect(0, 0, modes[1].width, modes[1].height);
    QCOMPARE(screens->count(), 1);
    QCOMPARE(screens->geometry(0), geo);
    QCOMPARE(static_cast<Screens*>(screens.data())->geometry(), geo);
    QCOMPARE(screens->size(0), geo.size());
    QCOMPARE(static_cast<Screens*>(screens.data())->size(), geo.size());
}

QTEST_GUILESS_MAIN(TestXRandRScreens)
#include "test_xrandr_screens.moc"
