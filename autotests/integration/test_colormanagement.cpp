/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/colorpipeline.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "outputconfigurationstore.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>
#include <format>

#include "qwayland-xx-color-management-v4.h"

using namespace std::chrono_literals;

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_color_management-0");

class ImageDescription : public QObject, public QtWayland::xx_image_description_v4
{
    Q_OBJECT
public:
    explicit ImageDescription(::xx_image_description_v4 *descr)
        : QtWayland::xx_image_description_v4(descr)
    {
    }

    ~ImageDescription() override
    {
        xx_image_description_v4_destroy(object());
    }

    void xx_image_description_v4_ready(uint32_t identity) override
    {
        Q_EMIT ready();
    }

    void xx_image_description_v4_failed(uint32_t cause, const QString &msg) override
    {
        Q_EMIT failed();
    }

Q_SIGNALS:
    void ready();
    void failed();
};

class ColorManagementSurface : public QObject, public QtWayland::xx_color_management_surface_v4
{
    Q_OBJECT
public:
    explicit ColorManagementSurface(::xx_color_management_surface_v4 *obj)
        : QtWayland::xx_color_management_surface_v4(obj)
    {
    }

    ~ColorManagementSurface() override
    {
        xx_color_management_surface_v4_destroy(object());
    }
};

class ColorManagementTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSetImageDescription_data();
    void testSetImageDescription();
    void testUnsupportedPrimaries();
    void testNoPrimaries();
    void testNoTf();
    void testNonsensePrimaries();
};

void ColorManagementTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void ColorManagementTest::init()
{
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::ColorManagement));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void ColorManagementTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ColorManagementTest::testSetImageDescription_data()
{
    QTest::addColumn<ColorDescription>("input");
    QTest::addColumn<bool>("protocolError");
    QTest::addColumn<bool>("shouldSucceed");

    QTest::addRow("sRGB") << ColorDescription::sRGB << false << true;
    QTest::addRow("rec.2020 PQ") << ColorDescription(NamedColorimetry::BT2020, TransferFunction(TransferFunction::PerceptualQuantizer), 203, 0, 400, 400) << false << true;
    QTest::addRow("scRGB") << ColorDescription(NamedColorimetry::BT709, TransferFunction(TransferFunction::linear, 0, 80), 80, 0, 80, 80) << false << true;
    QTest::addRow("custom") << ColorDescription(NamedColorimetry::BT2020, TransferFunction(TransferFunction::gamma22, 0.05, 400), 203, 0, 400, 400) << false << true;
    QTest::addRow("invalid tf") << ColorDescription(NamedColorimetry::BT2020, TransferFunction(TransferFunction::gamma22, 204, 205), 203, 0, 400, 400) << true << false;
    // TODO this should fail with the wp protocol version
    QTest::addRow("invalid HDR metadata") << ColorDescription(NamedColorimetry::BT2020, TransferFunction(TransferFunction::PerceptualQuantizer), 203, 500, 400, 400) << false << true;
    QTest::addRow("rec.2020 PQ with out of bounds white point") << ColorDescription(Colorimetry::fromName(NamedColorimetry::BT2020).withWhitepoint(xyY{0.9, 0.9, 1}), TransferFunction(TransferFunction::PerceptualQuantizer), 203, 0, 400, 400) << false << false;
}

void ColorManagementTest::testSetImageDescription()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto cmSurf = std::make_unique<ColorManagementSurface>(Test::colorManager()->get_surface(*surface));

    QtWayland::xx_image_description_creator_params_v4 creator(Test::colorManager()->new_parametric_creator());

    QFETCH(ColorDescription, input);

    creator.set_primaries(std::round(10'000 * input.containerColorimetry().red().toxyY().x),
                          std::round(10'000 * input.containerColorimetry().red().toxyY().y),
                          std::round(10'000 * input.containerColorimetry().green().toxyY().x),
                          std::round(10'000 * input.containerColorimetry().green().toxyY().y),
                          std::round(10'000 * input.containerColorimetry().blue().toxyY().x),
                          std::round(10'000 * input.containerColorimetry().blue().toxyY().y),
                          std::round(10'000 * input.containerColorimetry().white().toxyY().x),
                          std::round(10'000 * input.containerColorimetry().white().toxyY().y));
    switch (input.transferFunction().type) {
    case TransferFunction::sRGB:
        creator.set_tf_named(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB);
        creator.set_luminances(std::round(input.transferFunction().minLuminance * 10'000), std::round(input.transferFunction().maxLuminance), std::round(input.referenceLuminance()));
        break;
    case TransferFunction::gamma22:
        creator.set_tf_named(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22);
        creator.set_luminances(std::round(input.transferFunction().minLuminance * 10'000), std::round(input.transferFunction().maxLuminance), std::round(input.referenceLuminance()));
        break;
    case TransferFunction::linear:
        creator.set_tf_named(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR);
        creator.set_luminances(std::round(input.transferFunction().minLuminance * 10'000), std::round(input.transferFunction().maxLuminance), std::round(input.referenceLuminance()));
        break;
    case TransferFunction::PerceptualQuantizer:
        creator.set_tf_named(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ);
        creator.set_max_fall(std::round(input.maxAverageLuminance().value_or(0)));
        creator.set_max_cll(std::round(input.maxHdrLuminance().value_or(0)));
        break;
    }
    creator.set_mastering_luminance(std::round(input.minLuminance() * 10'000), std::round(input.maxHdrLuminance().value_or(0)));

    ImageDescription imageDescr(xx_image_description_creator_params_v4_create(creator.object()));

    QFETCH(bool, protocolError);
    if (protocolError) {
        QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
        QVERIFY(error.wait(50ms));
        return;
    }

    QFETCH(bool, shouldSucceed);
    if (shouldSucceed) {
        QSignalSpy ready(&imageDescr, &ImageDescription::ready);
        QVERIFY(ready.wait(50ms));

        cmSurf->set_image_description(imageDescr.object(), XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);

        QSignalSpy colorChange(window->surface(), &SurfaceInterface::colorDescriptionChanged);
        QVERIFY(colorChange.wait());

        QCOMPARE(window->surface()->colorDescription(), input);
    } else {
        QSignalSpy fail(&imageDescr, &ImageDescription::failed);
        QVERIFY(fail.wait(50ms));

        // trying to use a failed image description should cause an error
        QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
        cmSurf->set_image_description(imageDescr.object(), XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL);
        QVERIFY(error.wait(50ms));
    }
}

void ColorManagementTest::testUnsupportedPrimaries()
{
    QtWayland::xx_image_description_creator_params_v4 creator = QtWayland::xx_image_description_creator_params_v4(Test::colorManager()->new_parametric_creator());
    creator.set_primaries_named(-1);
    xx_image_description_creator_params_v4_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testNoPrimaries()
{
    QtWayland::xx_image_description_creator_params_v4 creator = QtWayland::xx_image_description_creator_params_v4(Test::colorManager()->new_parametric_creator());
    creator.set_tf_named(XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR);
    xx_image_description_creator_params_v4_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testNoTf()
{
    QtWayland::xx_image_description_creator_params_v4 creator = QtWayland::xx_image_description_creator_params_v4(Test::colorManager()->new_parametric_creator());
    creator.set_primaries_named(XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ);
    xx_image_description_creator_params_v4_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testNonsensePrimaries()
{
    QtWayland::xx_image_description_creator_params_v4 creator = QtWayland::xx_image_description_creator_params_v4(Test::colorManager()->new_parametric_creator());
    creator.set_primaries(0, 0, 0, 0, 0, 0, 0, 0);
    xx_image_description_creator_params_v4_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ColorManagementTest)
#include "test_colormanagement.moc"
