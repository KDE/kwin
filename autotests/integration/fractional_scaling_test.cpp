/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/surface.h>

#include <QDBusConnection>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_fractionalScale-0");

class TestFractionalScale : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testShow();
};

void TestFractionalScale::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(),
                              "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)),
                              Q_ARG(QVector<qreal>, QVector<qreal>() << 1.25 << 2.0));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1024, 819));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 640, 512));
    QCOMPARE(outputs[0]->scale(), 1.25);
    QCOMPARE(outputs[1]->scale(), 2.0);
}

void TestFractionalScale::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::FractionalScaleManagerV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    // put mouse in the middle of screen one
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TestFractionalScale::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestFractionalScale::testShow()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(fractionalScale->preferredScale(), 1.25 * 120);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    QCOMPARE(fractionalScale->preferredScale(), 1.25 * 120);
}

WAYLANDTEST_MAIN(TestFractionalScale)
#include "fractional_scaling_test.moc"
