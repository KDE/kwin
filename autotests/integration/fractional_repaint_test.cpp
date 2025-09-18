/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "backends/virtual/virtual_egl_backend.h"
#include "compositor.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_fractional_repaint-0");

class FractionalRepaintTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void testBottomRow();
};

void FractionalRepaintTest::initTestCase()
{
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::PresentationTime));

    // default config of a OnePlus 6
    Test::setOutputConfig({Test::OutputInfo{
        .scale = 2.65,
        .modes = {{QSize(1080, 2280), 60'000ul, OutputMode::Flag::Preferred}},
    }});

    // make sure open/close effects don't get in the way
    // of image comparisons
    effects->unloadAllEffects();
}

void FractionalRepaintTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void FractionalRepaintTest::testBottomRow()
{
    // this test verifies that with odd resolution + scale factor combinations,
    // the row of pixels "rounded away" by the logical coordinate grid still gets
    // repainted properly and doesn't have any glitches in it
    Output *output = workspace()->outputs().front();
    QCOMPARE(output->geometry().height(), 860);
    QCOMPARE(output->geometry().height() * output->scale(), 2279);
    QCOMPARE(output->geometryF().height() * output->scale(), 2280);

    // render a window that goes beyond the integer logical part of the screen
    auto surface = Test::createSurface();
    auto toplevel = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(1080, 861), Qt::red);
    window->move(output->geometry().topLeft());

    Cursors::self()->hideCursor();

    // render a few frames, to make sure the swapchain is entirely red
    for (int i = 0; i < 3; i++) {
        auto feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        QVERIFY(spy.wait());
    }

    const auto layer = static_cast<VirtualEglLayer *>(Compositor::self()->backend()->compatibleOutputLayers(output).front());

    QImage img(QSize(1080, 2280), QImage::Format::Format_RGBA8888_Premultiplied);
    img.fill(Qt::red);
    QCOMPARE(layer->texture()->toImage(), img);

    // maximize the window
    window->maximize(MaximizeMode::MaximizeFull);
    QSignalSpy toplevelConfigure(toplevel.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigure(toplevel->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(toplevelConfigure.wait());
    QCOMPARE(toplevelConfigure.last().at(0).toSize(), QSize(408, 860));
    toplevel->xdgSurface()->ack_configure(surfaceConfigure.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigure.last().at(0).toSize(), Qt::blue);

    {
        auto feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        QVERIFY(spy.wait());
    }

    img.fill(Qt::blue);
    // the last row of pixels should be black, not red
    for (int i = 0; i < img.width(); i++) {
        img.setPixel(i, 2279, qRgba(0, 0, 0, 255));
    }
    QCOMPARE(layer->texture()->toImage(), img);
}

}

WAYLANDTEST_MAIN(KWin::FractionalRepaintTest)
#include "fractional_repaint_test.moc"
