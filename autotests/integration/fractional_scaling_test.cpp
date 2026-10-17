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
#include <QScopeGuard>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>

using namespace KWin;

class TestFractionalScale : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testToplevel();
    void testPopup();
    void testWindowFillingTheOutputReachesItsEdges_data();
    void testWindowFillingTheOutputReachesItsEdges();
};

void TestFractionalScale::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = Rect(0, 0, 1280 / 1.25, 1024 / 1.25),
            .scale = 1.25,
        },
        Test::OutputInfo{
            .geometry = Rect(1280, 0, 1280 / 2, 1024 / 2),
            .scale = 2.0,
        },
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), Rect(0, 0, 1024, 819));
    QCOMPARE(outputs[1]->geometry(), Rect(1280, 0, 640, 512));
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

void TestFractionalScale::testWindowFillingTheOutputReachesItsEdges_data()
{
    QTest::addColumn<QSize>("pixelSize");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("fullScreen");

    // At these scales the output is not a whole number of logical pixels wide or high. Rounding
    // that size down to whole logical pixels loses more than half a device pixel.
    QTest::addRow("maximized at 165%%") << QSize(2880, 1800) << 1.65 << false;
    QTest::addRow("maximized at 190%%") << QSize(2880, 1800) << 1.9 << false;
    QTest::addRow("maximized at 210%%") << QSize(2880, 1800) << 2.1 << false;
    QTest::addRow("maximized at 125%%, whole logical size") << QSize(2880, 1800) << 1.25 << false;
    QTest::addRow("fullscreen at 165%%") << QSize(2880, 1800) << 1.65 << true;
    QTest::addRow("fullscreen at 190%%") << QSize(2880, 1800) << 1.9 << true;
}

void TestFractionalScale::testWindowFillingTheOutputReachesItsEdges()
{
    // A window that fills the output covers it to the last device pixel. A window one device pixel
    // short leaves a line of whatever is below it at the right or bottom edge, and the pointer at
    // that edge misses the window.
    QFETCH(QSize, pixelSize);
    QFETCH(qreal, scale);
    QFETCH(bool, fullScreen);

    const auto restoreOutputs = qScopeGuard([] {
        Test::setOutputConfig({
            Test::OutputInfo{
                .geometry = Rect(0, 0, 1280 / 1.25, 1024 / 1.25),
                .scale = 1.25,
            },
            Test::OutputInfo{
                .geometry = Rect(1280, 0, 1280 / 2, 1024 / 2),
                .scale = 2.0,
            },
        });
    });
    // The helper derives the mode from a whole logical size, so give the output its exact mode.
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = Rect(QPoint(0, 0), (QSizeF(pixelSize) / scale).toSize()),
            .scale = scale,
            .modes = {OutputModeline(pixelSize, 60000, OutputModeline::Flag::Preferred)},
        },
    });
    LogicalOutput *output = workspace()->outputs().constFirst();
    QCOMPARE(output->pixelSize(), pixelSize);
    workspace()->setActiveOutput(output);
    KWin::input()->pointer()->warp(output->geometryF().center());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::FractionalScaleV1> fractionalScale(Test::createFractionalScaleV1(surface.get()));
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(300, 200), Qt::blue);
    QVERIFY(window);
    // The configure that makes the window active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    if (fullScreen) {
        window->setFullScreen(true);
    } else {
        window->maximize(MaximizeFull);
    }
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    const QSize configuredSize = toplevelConfigureRequestedSpy.last().at(0).toSize();
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), configuredSize, Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    const RectF frame = window->frameGeometry();
    const QString geometry = QStringLiteral("frame %1,%2 %3x%4 for an output of %5x%6 device pixels, configured %7x%8")
                                 .arg(frame.x())
                                 .arg(frame.y())
                                 .arg(frame.width())
                                 .arg(frame.height())
                                 .arg(pixelSize.width())
                                 .arg(pixelSize.height())
                                 .arg(configuredSize.width())
                                 .arg(configuredSize.height());
    qDebug().noquote() << geometry;
    QVERIFY2(std::round(frame.left() * scale) <= 0 && std::round(frame.top() * scale) <= 0, qPrintable(geometry));
    QVERIFY2(std::round(frame.right() * scale) >= pixelSize.width(), qPrintable(geometry));
    QVERIFY2(std::round(frame.bottom() * scale) >= pixelSize.height(), qPrintable(geometry));
}

WAYLANDTEST_MAIN(TestFractionalScale)
#include "fractional_scaling_test.moc"
