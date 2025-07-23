#include "compositor.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "kwin_wayland_test.h"
#include "wayland_server.h"
#include "workspace.h"

#include <ranges>

namespace KWin
{

static const QString s_socketName = QStringLiteral("test_drm");

class DrmTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void initTestCase();
    void cleanup();

    void testModesets();
    void testPresentation();
    void testCursorLayer();
    void testDirectScanout();
    void testOverlay();
};

void DrmTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PresentationTime));
}

void DrmTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    QVERIFY(!allOutputs.isEmpty());

    // Create a simple setup that's useful for most tests.
    OutputConfiguration cfg;
    cfg.changeSet(allOutputs.front())->enabled = true;
    cfg.changeSet(allOutputs.front())->scale = 1;
    for (Output *output : allOutputs | std::views::drop(1)) {
        cfg.changeSet(output)->enabled = false;
    }
    QCOMPARE(workspace()->applyOutputConfiguration(cfg), OutputConfigurationError::None);
}

void DrmTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DrmTest::testModesets()
{
    // show a window for tracking presentation
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    Output *output = kwinApp()->outputBackend()->outputs().front();
    window->move(output->geometryF().center());

    // verify that we can cycle through modes
    std::unique_ptr<Test::WpPresentationFeedback> feedback;
    for (const auto &mode : output->modes()) {
        OutputConfiguration cfg;
        cfg.changeSet(output)->mode = mode;
        QCOMPARE(workspace()->applyOutputConfiguration(cfg), OutputConfigurationError::None);

        feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        QVERIFY(spy.wait());
    }
}

void DrmTest::testPresentation()
{
    // tests normal presentation
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    Output *output = kwinApp()->outputBackend()->outputs().front();
    window->move(output->geometryF().center());

    std::unique_ptr<Test::WpPresentationFeedback> feedback;
    for (int i = 0; i < 5; i++) {
        feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        QVERIFY(spy.wait());
    }
}

void DrmTest::testCursorLayer()
{
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    window->move(output->geometryF().center());
    std::unique_ptr<Test::WpPresentationFeedback> feedback;

    // if there's a visible cursor, a non-primary plane should be used to present it
    Test::pointerMotion(output->geometryF().center(), time++);

    feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
    QVERIFY(spy.wait());

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 2);
}

void DrmTest::testDirectScanout()
{
    QSKIP("Need the test client to attach a dmabuf and implement dmabuf feedback first...");
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), [](Test::XdgToplevel *toplevel) {
        toplevel->set_fullscreen(nullptr);
    }));
    auto window = Test::renderAndWaitForShown(surface.get(), output->pixelSize(), Qt::blue);
    QVERIFY(window);
    window->move(output->geometryF().center());
    std::unique_ptr<Test::WpPresentationFeedback> feedback;

    // if there's a visible cursor, a non-primary plane should be used to present it
    Test::pointerMotion(output->geometryF().center(), time++);

    feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
    QVERIFY(spy.wait());

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 2);
    // TODO check here that the client buffer is actually used on the plane
}

void DrmTest::testOverlay()
{
    QSKIP("Need the test client to attach a dmabuf and implement dmabuf feedback first...");
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() < 3) {
        QSKIP("The driver doesn't advertise an overlay plane");
    }

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    window->move(output->geometryF().center());
    std::unique_ptr<Test::WpPresentationFeedback> feedback;

    // if there's a visible cursor, a non-primary plane should be used to present it
    Test::pointerMotion(output->geometryF().center(), time++);

    // emulate a quickly updating surface
    for (int i = 0; i < 100; i++) {
        feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->damage(QRect(0, 0, 10, 10));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        QVERIFY(spy.wait());
    }

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 3);
}
}

WAYLAND_DRM_TEST_MAIN(KWin::DrmTest)

#include "drm_test.moc"
