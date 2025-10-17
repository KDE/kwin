/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/colorpipeline.h"
#include "core/drmdevice.h"
#include "core/graphicsbuffer.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "outputconfigurationstore.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>
#include <drm_fourcc.h>
#include <format>

#include "qwayland-color-management-v1.h"
#include "wayland-linux-dmabuf-unstable-v1-client-protocol.h"

using namespace std::chrono_literals;

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_color_management-0");

class ImageDescription : public QObject, public QtWayland::wp_image_description_v1
{
    Q_OBJECT
public:
    explicit ImageDescription(::wp_image_description_v1 *descr)
        : QtWayland::wp_image_description_v1(descr)
    {
    }

    ~ImageDescription() override
    {
        wp_image_description_v1_destroy(object());
    }

    void wp_image_description_v1_ready(uint32_t identity) override
    {
        Q_EMIT ready();
    }

    void wp_image_description_v1_failed(uint32_t cause, const QString &msg) override
    {
        Q_EMIT failed();
    }

Q_SIGNALS:
    void ready();
    void failed();
};

class ColorManagementSurface : public QObject, public QtWayland::wp_color_management_surface_v1
{
    Q_OBJECT
public:
    explicit ColorManagementSurface(::wp_color_management_surface_v1 *obj)
        : QtWayland::wp_color_management_surface_v1(obj)
    {
    }

    ~ColorManagementSurface() override
    {
        wp_color_management_surface_v1_destroy(object());
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
    void testRenderIntentOnly();
    void testInertError();
};

void ColorManagementTest::initTestCase()
{
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
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
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::ColorManagement
                                         | Test::AdditionalWaylandInterface::ColorRepresentation
                                         | Test::AdditionalWaylandInterface::LinuxDmabuf));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void ColorManagementTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ColorManagementTest::testSetImageDescription_data()
{
    QTest::addColumn<std::shared_ptr<ColorDescription>>("input");
    QTest::addColumn<RenderingIntent>("renderingIntent");
    QTest::addColumn<bool>("protocolError");
    QTest::addColumn<bool>("shouldSucceed");
    QTest::addColumn<std::optional<std::shared_ptr<ColorDescription>>>("expectedResult");

    // sRGB is not tested, because it's the default (and thus no change signal will be emitted)
    QTest::addRow("rec.2020 PQ")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("scRGB")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT709,
               TransferFunction(TransferFunction::linear, 0, 80),
               80,
               0,
               80,
               80,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("custom")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::gamma22, 0.05, 400),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("invalid tf")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::gamma22, 204, 205),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << true << false
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("invalid HDR metadata")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               500,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << true << false
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("rec.2020 PQ with out of bounds white point")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020.withWhitepoint(xyY{0.9, 0.9, 1}),
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << false << false
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("nonsense primaries")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry(xy{0, 0}, xy{0, 0}, xy{0, 0}, xy{0, 0}),
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << false << false
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("custom PQ luminances are ignored")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer, 10, 100),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional(std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer, 10, 10'010),
               203,
               0,
               400,
               400,
           }));

    QTest::addRow("rec.2020 PQ relative colorimetric")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::RelativeColorimetric
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("rec.2020 PQ relative colorimetric bpc")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT2020,
               TransferFunction(TransferFunction::PerceptualQuantizer),
               203,
               0,
               400,
               400,
           })
        << RenderingIntent::RelativeColorimetricWithBPC
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    // TODO uncomment this once a matching rendering intent is added to the Wayland protocol
    // QTest::addRow("rec.2020 PQ absolute colorimetric")
    //     << std::make_shared<ColorDescription>(ColorDescription{
    //            Colorimetry::BT2020,
    //            TransferFunction(TransferFunction::PerceptualQuantizer),
    //            203,
    //            0,
    //            400,
    //            400,
    //        })
    //     << RenderingIntent::AbsoluteColorimetricNoAdaptation
    //     << false << true
    //     << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("rec.709 + BT1886")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT709,
               TransferFunction(TransferFunction::BT1886),
               100,
               0.1,
               100,
               100,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();

    QTest::addRow("rec601 limited range YUV")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT709,
               TransferFunction(TransferFunction::BT1886),
               YUVMatrixCoefficients::BT601,
               EncodingRange::Limited,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("rec709 full range YUV")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT709,
               TransferFunction(TransferFunction::BT1886),
               YUVMatrixCoefficients::BT709,
               EncodingRange::Full,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
    QTest::addRow("rec2020 limited range YUV")
        << std::make_shared<ColorDescription>(ColorDescription{
               Colorimetry::BT709,
               TransferFunction(TransferFunction::PerceptualQuantizer),
               YUVMatrixCoefficients::BT2020,
               EncodingRange::Limited,
           })
        << RenderingIntent::Perceptual
        << false << true
        << std::optional<std::shared_ptr<ColorDescription>>();
}

static ImageDescription createImageDescription(ColorManagementSurface *surface, const ColorDescription &color)
{
    QtWayland::wp_image_description_creator_params_v1 creator(Test::colorManager()->create_parametric_creator());

    creator.set_primaries(std::round(1'000'000.0 * color.containerColorimetry().red().toxyY().x),
                          std::round(1'000'000.0 * color.containerColorimetry().red().toxyY().y),
                          std::round(1'000'000.0 * color.containerColorimetry().green().toxyY().x),
                          std::round(1'000'000.0 * color.containerColorimetry().green().toxyY().y),
                          std::round(1'000'000.0 * color.containerColorimetry().blue().toxyY().x),
                          std::round(1'000'000.0 * color.containerColorimetry().blue().toxyY().y),
                          std::round(1'000'000.0 * color.containerColorimetry().white().toxyY().x),
                          std::round(1'000'000.0 * color.containerColorimetry().white().toxyY().y));
    switch (color.transferFunction().type) {
    case TransferFunction::sRGB:
        creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
        break;
    case TransferFunction::gamma22:
        creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
        break;
    case TransferFunction::linear:
        creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR);
        break;
    case TransferFunction::PerceptualQuantizer:
        creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
        break;
    case TransferFunction::BT1886:
        creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886);
        break;
    }
    creator.set_luminances(std::round(color.transferFunction().minLuminance * 10'000), std::round(color.transferFunction().maxLuminance), std::round(color.referenceLuminance()));
    creator.set_max_fall(std::round(color.maxAverageLuminance().value_or(0)));
    creator.set_max_cll(std::round(color.maxHdrLuminance().value_or(0)));
    creator.set_mastering_luminance(std::round(color.minLuminance() * 10'000), std::round(color.maxHdrLuminance().value_or(0)));

    return ImageDescription(wp_image_description_creator_params_v1_create(creator.object()));
}

static const std::unordered_map<YUVMatrixCoefficients, wp_color_representation_surface_v1_coefficients> s_coefficientsMap = {
    {YUVMatrixCoefficients::Identity, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY},
    {YUVMatrixCoefficients::BT601, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601},
    {YUVMatrixCoefficients::BT709, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709},
    {YUVMatrixCoefficients::BT2020, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020},
};
static const std::unordered_map<EncodingRange, wp_color_representation_surface_v1_range> s_rangeMap = {
    {EncodingRange::Limited, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED},
    {EncodingRange::Full, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL},
};

static std::unique_ptr<Test::ColorRepresentationSurfaceV1> createRepresentation(KWayland::Client::Surface *surf,
                                                                                const std::shared_ptr<ColorDescription> &color)
{
    auto ret = std::make_unique<Test::ColorRepresentationSurfaceV1>(Test::colorRepresentation()->get_surface(*surf));
    ret->set_coefficients_and_range(s_coefficientsMap.at(color->yuvCoefficients()), s_rangeMap.at(color->range()));
    return ret;
}

static wl_buffer *createYuvBuffer(const QSize &size)
{
    auto device = DrmDevice::open(Test::linuxDmabuf()->mainDevice());
    GraphicsBufferRef Y = device->allocator()->allocate(GraphicsBufferOptions{
        .size = size,
        .format = DRM_FORMAT_R8,
        .modifiers = {DRM_FORMAT_MOD_LINEAR},
        .software = false,
    });
    Q_ASSERT(Y.buffer());
    GraphicsBufferRef CrCb = device->allocator()->allocate(GraphicsBufferOptions{
        .size = size / 2,
        .format = DRM_FORMAT_GR88,
        .modifiers = {DRM_FORMAT_MOD_LINEAR},
        .software = false,
    });
    Q_ASSERT(CrCb.buffer());
    return Test::linuxDmabuf()->importBuffer(DRM_FORMAT_NV12, {Y.buffer(), CrCb.buffer()});
}

void ColorManagementTest::testSetImageDescription()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto cmSurf = std::make_unique<ColorManagementSurface>(Test::colorManager()->get_surface(*surface));

    QFETCH(std::shared_ptr<ColorDescription>, input);
    QFETCH(RenderingIntent, renderingIntent);

    ImageDescription imageDescr = createImageDescription(cmSurf.get(), *input);
    auto representation = createRepresentation(surface.get(), input);

    // YUV color descriptions require YUV buffers
    if (input->yuvCoefficients() != YUVMatrixCoefficients::Identity) {
        auto buffer = createYuvBuffer(QSize(100, 50));
        surface->attachBuffer(buffer);
        wl_buffer_destroy(buffer);
    }

    QFETCH(bool, protocolError);
    if (protocolError) {
        QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
        QVERIFY(error.wait(50ms));
        return;
    }

    uint32_t waylandRenderIntent;
    switch (renderingIntent) {
    case RenderingIntent::Perceptual:
        waylandRenderIntent = WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL;
        break;
    case RenderingIntent::RelativeColorimetric:
        waylandRenderIntent = WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE;
        break;
    case RenderingIntent::RelativeColorimetricWithBPC:
        waylandRenderIntent = WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE_BPC;
        break;
    case RenderingIntent::AbsoluteColorimetricNoAdaptation:
        Q_UNREACHABLE();
        break;
    default:
        Q_UNREACHABLE();
    }

    QFETCH(bool, shouldSucceed);
    QFETCH(std::optional<std::shared_ptr<ColorDescription>>, expectedResult);
    if (shouldSucceed) {
        QSignalSpy ready(&imageDescr, &ImageDescription::ready);
        QVERIFY(ready.wait(50ms));

        cmSurf->set_image_description(imageDescr.object(), waylandRenderIntent);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);

        QSignalSpy colorChange(window->surface(), &SurfaceInterface::colorDescriptionChanged);
        QVERIFY(colorChange.wait());

        QCOMPARE(*window->surface()->colorDescription(), *expectedResult.value_or(input));
        QCOMPARE(window->surface()->renderingIntent(), renderingIntent);
    } else {
        QSignalSpy fail(&imageDescr, &ImageDescription::failed);
        QVERIFY(fail.wait(50ms));

        // trying to use a failed image description should cause an error
        QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
        cmSurf->set_image_description(imageDescr.object(), waylandRenderIntent);
        QVERIFY(error.wait(50ms));
    }
}

void ColorManagementTest::testUnsupportedPrimaries()
{
    QtWayland::wp_image_description_creator_params_v1 creator = QtWayland::wp_image_description_creator_params_v1(Test::colorManager()->create_parametric_creator());
    creator.set_primaries_named(-1);
    wp_image_description_creator_params_v1_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testNoPrimaries()
{
    QtWayland::wp_image_description_creator_params_v1 creator = QtWayland::wp_image_description_creator_params_v1(Test::colorManager()->create_parametric_creator());
    creator.set_tf_named(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR);
    wp_image_description_creator_params_v1_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testNoTf()
{
    QtWayland::wp_image_description_creator_params_v1 creator = QtWayland::wp_image_description_creator_params_v1(Test::colorManager()->create_parametric_creator());
    creator.set_primaries_named(WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ);
    wp_image_description_creator_params_v1_create(creator.object());
    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

void ColorManagementTest::testRenderIntentOnly()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    const auto color = std::make_shared<ColorDescription>(ColorDescription{
        Colorimetry::BT2020,
        TransferFunction(TransferFunction::PerceptualQuantizer),
    });

    auto cmSurf = std::make_unique<ColorManagementSurface>(Test::colorManager()->get_surface(*surface));
    ImageDescription imageDescr = createImageDescription(cmSurf.get(), *color);

    QSignalSpy ready(&imageDescr, &ImageDescription::ready);
    QVERIFY(ready.wait(50ms));

    cmSurf->set_image_description(imageDescr.object(), WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy colorChange(window->surface(), &SurfaceInterface::colorDescriptionChanged);
    QVERIFY(colorChange.wait());

    QCOMPARE(*window->surface()->colorDescription(), *color);
    QCOMPARE(window->surface()->renderingIntent(), RenderingIntent::Perceptual);

    // only changing the rendering intent should also trigger the colorDescriptionChanged signal to be emitted
    cmSurf->set_image_description(imageDescr.object(), WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(colorChange.wait());
    QCOMPARE(window->surface()->colorDescription(), color);
    QCOMPARE(window->surface()->renderingIntent(), RenderingIntent::RelativeColorimetric);
}

void ColorManagementTest::testInertError()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    auto cmSurf = std::make_unique<ColorManagementSurface>(Test::colorManager()->get_surface(*surface));
    ImageDescription imageDescr = createImageDescription(cmSurf.get(), *ColorDescription::sRGB);

    QSignalSpy ready(&imageDescr, &ImageDescription::ready);
    QVERIFY(ready.wait(50ms));

    surface.reset();
    cmSurf->set_image_description(imageDescr.object(), WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ColorManagementTest)
#include "test_colormanagement.moc"
