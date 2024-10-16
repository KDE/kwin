/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
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

    void testToplevel();
    void testPopup();
};

void TestFractionalScale::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280 / 1.25, 1024 / 1.25),
            .scale = 1.25,
        },
        Test::OutputInfo{
            .geometry = QRect(1280, 0, 1280 / 2, 1024 / 2),
            .scale = 2.0,
        },
    });

    kwinApp()->start();
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
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TestFractionalScale::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestFractionalScale::testToplevel()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    QSignalSpy fractionalScaleChanged(fractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(fractionalScaleChanged.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.25 * 120));

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QCOMPARE(fractionalScaleChanged.count(), 1);
    QCOMPARE(fractionalScale->preferredScale(), std::round(1.25 * 120));

    // move to screen 2
    window->move(QPoint(1280, 0));

    QVERIFY(Test::waylandSync());
    QCOMPARE(fractionalScaleChanged.count(), 2);
    QCOMPARE(fractionalScale->preferredScale(), std::round(2.0 * 120));
}

void TestFractionalScale::testPopup()
{
    std::unique_ptr<KWayland::Client::Surface> toplevelSurface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> toplevelFractionalScale(Test::createFractionalScaleV1(toplevelSurface.get()));
    QSignalSpy toplevelFractionalScaleChanged(toplevelFractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgToplevel> toplevel(Test::createXdgToplevelSurface(toplevelSurface.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(toplevelFractionalScaleChanged.count(), 1);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(1.25 * 120));

    auto toplevelWindow = Test::renderAndWaitForShown(toplevelSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(toplevelWindow);
    QCOMPARE(toplevelFractionalScaleChanged.count(), 1);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(1.25 * 120));

    std::unique_ptr<KWayland::Client::Surface> popupSurface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> popupFractionalScale(Test::createFractionalScaleV1(popupSurface.get()));
    QSignalSpy popupFractionalScaleChanged(popupFractionalScale.get(), &Test::FractionalScaleV1::preferredScaleChanged);
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(10, 10, 10, 10);
    std::unique_ptr<Test::XdgPopup> popup(Test::createXdgPopupSurface(popupSurface.get(), toplevel->xdgSurface(), positioner.get()));

    // above call commits the surface and blocks for the configure event. We should have received the scale already
    // We are sent the value in 120ths
    QCOMPARE(popupFractionalScaleChanged.count(), 1);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(1.25 * 120));

    auto popupWindow = Test::renderAndWaitForShown(popupSurface.get(), QSize(10, 10), Qt::cyan);
    QVERIFY(popupWindow);
    QCOMPARE(popupFractionalScaleChanged.count(), 1);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(1.25 * 120));

    // move the parent to screen 2
    toplevelWindow->move(QPoint(1280, 0));

    QVERIFY(Test::waylandSync());
    QCOMPARE(toplevelFractionalScaleChanged.count(), 2);
    QCOMPARE(toplevelFractionalScale->preferredScale(), std::round(2.0 * 120));
    QCOMPARE(popupFractionalScaleChanged.count(), 2);
    QCOMPARE(popupFractionalScale->preferredScale(), std::round(2.0 * 120));
}

WAYLANDTEST_MAIN(TestFractionalScale)
#include "fractional_scaling_test.moc"
