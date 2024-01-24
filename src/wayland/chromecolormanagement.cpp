#include "chromecolormanagement.h"
#include "display.h"
#include "output.h"
#include "surface.h"
#include "surface_p.h"
#include "utils/resource.h"
#include "workspace.h"

namespace KWin
{

static constexpr uint32_t s_version = 6;

ChromeColorManagementManager::ChromeColorManagementManager(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::zcr_color_manager_v1(*display, s_version)
{
}

void ChromeColorManagementManager::zcr_color_manager_v1_create_color_space_from_icc(Resource *resource, uint32_t id, int32_t icc)
{
    qWarning() << __func__ << "is not implemented!";
}

void ChromeColorManagementManager::zcr_color_manager_v1_create_color_space_from_names(Resource *resource, uint32_t id, uint32_t eotf, uint32_t chromaticity, uint32_t whitepoint)
{
    qWarning("zcr_color_manager_v1_create_color_space_from_names is deprecated and not implemented");
}

void ChromeColorManagementManager::zcr_color_manager_v1_create_color_space_from_params(Resource *resource, uint32_t id, uint32_t eotf, uint32_t primary_r_x, uint32_t primary_r_y, uint32_t primary_g_x, uint32_t primary_g_y, uint32_t primary_b_x, uint32_t primary_b_y, uint32_t white_point_x, uint32_t white_point_y)
{
    qWarning("zcr_color_manager_v1_create_color_space_from_params is deprecated and not implemented");
}

void ChromeColorManagementManager::zcr_color_manager_v1_get_color_management_output(Resource *resource, uint32_t id, struct ::wl_resource *output)
{
    auto o = OutputInterface::get(output);
    if (!o) {
        wl_resource_post_error(resource->handle, 0, "output doesn't exist");
        return;
    }
    new ChromeColorManagementOutput(resource->client(), id, o->handle());
}

void ChromeColorManagementManager::zcr_color_manager_v1_get_color_management_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(surf);
    if (surfPriv->chromeColorManagement || surfPriv->frogColorManagement) {
        wl_resource_post_error(resource->handle, 0, "surface already uses a color management protocol");
        return;
    }
    surfPriv->chromeColorManagement = new ChromeColorManagementSurface(resource->client(), id, surf);
}

static NamedTransferFunction tfCodeToKWinTF(uint32_t name)
{
    switch (name) {
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_srgb:
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_bt709:
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_smpte170m:
        return NamedTransferFunction::gamma22;
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_scrgb_linear_80_nits:
        return NamedTransferFunction::scRGB;
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_pq:
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_bt2020_10:
    case QtWaylandServer::zcr_color_manager_v1::eotf_names_bt2020_12:
        return NamedTransferFunction::PerceptualQuantizer;
    default:
        qWarning() << "unrecognized transfer function" << name;
        return NamedTransferFunction::linear;
    }
}

void ChromeColorManagementManager::zcr_color_manager_v1_create_color_space_from_complete_names(Resource *resource, uint32_t id, uint32_t eotf, uint32_t chromaticity, uint32_t whitepoint, uint32_t matrix, uint32_t range)
{
    Colorimetry color = Colorimetry::fromName(NamedColorimetry::BT709);
    switch (chromaticity) {
    case chromaticity_names_bt709:
        break;
    case chromaticity_names_bt2020:
        color = Colorimetry::fromName(NamedColorimetry::BT2020);
        break;
    case chromaticity_names_bt601_625_line:
        color = Colorimetry(
            QVector2D(0.640, 0.330),
            QVector2D(0.290, 0.600),
            QVector2D(0.150, 0.060),
            QVector2D(0.3127, 0.3290));
        break;
    case chromaticity_names_bt601_525_line:
    case chromaticity_names_smpte170m:
        color = Colorimetry(
            QVector2D(0.630, 0.340),
            QVector2D(0.310, 0.595),
            QVector2D(0.155, 0.070),
            QVector2D(0.3127, 0.3290));
        break;
    case chromaticity_names_displayp3:
        color = Colorimetry(
            QVector2D(0.680, 0.320),
            QVector2D(0.265, 0.690),
            QVector2D(0.150, 0.060),
            QVector2D(0.3127, 0.3290));
        break;
    default:
        qWarning() << "unrecognized colorimetry" << chromaticity;
    }
    switch (whitepoint) {
    case whitepoint_names_d50:
        color = Colorimetry(color.red(), color.green(), color.blue(), QVector2D(0.34567, 0.35850));
        break;
    case whitepoint_names_d65:
        color = Colorimetry(color.red(), color.green(), color.blue(), Colorimetry::fromName(NamedColorimetry::BT709).white());
        break;
    default:
        qWarning() << "unrecognized whitepoint" << whitepoint;
    }
    const auto colorspace = new ChromeColorManagementColorSpace(resource->client(), 0, ColorDescription(color, tfCodeToKWinTF(eotf), 100, 0, 600, 1000, 0));
    ChromeColorManagementColorSpaceCreator tmp(resource->client(), id, colorspace);
}

void ChromeColorManagementManager::zcr_color_manager_v1_create_color_space_from_complete_params(Resource *resource, uint32_t id, uint32_t eotf, uint32_t matrix, uint32_t range, uint32_t primary_r_x, uint32_t primary_r_y, uint32_t primary_g_x, uint32_t primary_g_y, uint32_t primary_b_x, uint32_t primary_b_y, uint32_t white_point_x, uint32_t white_point_y)
{
    Colorimetry color(
        QVector2D(primary_r_x / 10'000.0, primary_r_y / 10'000.0),
        QVector2D(primary_g_x / 10'000.0, primary_g_y / 10'000.0),
        QVector2D(primary_b_x / 10'000.0, primary_b_y / 10'000.0),
        QVector2D(white_point_x / 10'000.0, white_point_y / 10'000.0));
    const auto colorspace = new ChromeColorManagementColorSpace(resource->client(), 0, ColorDescription(color, tfCodeToKWinTF(eotf), 100, 0, 600, 1000, 0));
    ChromeColorManagementColorSpaceCreator tmp(resource->client(), id, colorspace);
}

void ChromeColorManagementManager::zcr_color_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

ChromeColorManagementOutput::ChromeColorManagementOutput(wl_client *client, uint32_t id, Output *output)
    : QtWaylandServer::zcr_color_management_output_v1(client, id, s_version)
    , m_output(output)
{
    send_extended_dynamic_range(resource()->handle, 2000);
    send_color_space_changed(resource()->handle);
    connect(output, &Output::wideColorGamutChanged, this, [this]() {
        send_color_space_changed(resource()->handle);
    });
}

void ChromeColorManagementOutput::zcr_color_management_output_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ChromeColorManagementOutput::zcr_color_management_output_v1_get_color_space(Resource *resource, uint32_t id)
{
    if (m_output) {
        qWarning() << "sending rec709?" << (m_output->colorDescription().colorimetry() == NamedColorimetry::BT709) << "or 2020?" << (m_output->colorDescription().colorimetry() == NamedColorimetry::BT2020);
        new ChromeColorManagementColorSpace(resource->client(), id, m_output->colorDescription());
    } else {
        new ChromeColorManagementColorSpace(resource->client(), id, ColorDescription::sRGB);
    }
}

void ChromeColorManagementOutput::zcr_color_management_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

ChromeColorManagementSurface::ChromeColorManagementSurface(wl_client *client, uint32_t id, SurfaceInterface *surface)
    : QtWaylandServer::zcr_color_management_surface_v1(client, id, s_version)
    , m_surface(surface)
{
}

ChromeColorManagementSurface::~ChromeColorManagementSurface()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->chromeColorManagement = nullptr;
        priv->pending->colorDescription = ColorDescription::sRGB;
        priv->pending->colorDescriptionIsSet = true;
    }
}

void ChromeColorManagementSurface::setPreferredColorDescription(const ColorDescription &descr)
{
    m_preferredDescription = descr;
    if (!m_surface || !SurfaceInterfacePrivate::get(m_surface)->primaryOutput) {
        return;
    }
    const auto resources = SurfaceInterfacePrivate::get(m_surface)->primaryOutput->clientResources(resource()->client());
    for (const auto &res : resources) {
        send_preferred_color_space(resource()->handle, res);
    }
}

void ChromeColorManagementSurface::zcr_color_management_surface_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode)
{
    qWarning() << __func__ << "is not implemented!";
}

void ChromeColorManagementSurface::zcr_color_management_surface_v1_set_extended_dynamic_range(Resource *resource, uint32_t value)
{
    qWarning() << __func__ << "is not implemented!";
}

void ChromeColorManagementSurface::zcr_color_management_surface_v1_set_color_space(Resource *resource, struct ::wl_resource *color_space, uint32_t render_intent)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ChromeColorManagementColorSpace::get(color_space)->colorDescription;
    priv->pending->colorDescriptionIsSet = true;
    qWarning() << "color description set to rec2020?" << (priv->pending->colorDescription.colorimetry() == NamedColorimetry::BT2020) << "or rec709?" << (priv->pending->colorDescription.colorimetry() == NamedColorimetry::BT709);
    qWarning() << "rendering intent" << render_intent << "ignored";
}

void ChromeColorManagementSurface::zcr_color_management_surface_v1_set_default_color_space(Resource *resource)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ColorDescription::sRGB;
    priv->pending->colorDescriptionIsSet = true;
}

void ChromeColorManagementSurface::zcr_color_management_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static uint32_t kwinTFToProtoTF(NamedTransferFunction tf)
{
    switch (tf) {
    case NamedTransferFunction::gamma22:
    case NamedTransferFunction::sRGB:
        return zcr_color_manager_v1_eotf_names::ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709;
    case NamedTransferFunction::scRGB:
        return zcr_color_manager_v1_eotf_names::ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SCRGB_LINEAR_80_NITS;
    case NamedTransferFunction::PerceptualQuantizer:
        return zcr_color_manager_v1_eotf_names::ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ;
    case NamedTransferFunction::linear:
        return zcr_color_manager_v1_eotf_names::ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR;
    }
    Q_UNREACHABLE();
}

static uint32_t kwinColorspaceToProtoMatrix(const Colorimetry &colorimetry)
{
    if (colorimetry == NamedColorimetry::BT709) {
        return ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT709;
    } else if (colorimetry == NamedColorimetry::BT2020) {
        return ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT2020_NCL;
    } else {
        return ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_RGB;
    }
}

ChromeColorManagementColorSpace::ChromeColorManagementColorSpace(wl_client *client, uint32_t id, const ColorDescription &colorDescription)
    : QtWaylandServer::zcr_color_space_v1(client, id, s_version)
    , colorDescription(colorDescription)
{
}

void ChromeColorManagementColorSpace::zcr_color_space_v1_get_information(Resource *resource)
{
    const auto p210k = [](float primary) {
        return std::round(primary * 10'000);
    };
    send_complete_params(
        resource->handle, kwinTFToProtoTF(colorDescription.transferFunction()), kwinColorspaceToProtoMatrix(colorDescription.colorimetry()), ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL,
        p210k(colorDescription.colorimetry().red().x()), p210k(colorDescription.colorimetry().red().y()),
        p210k(colorDescription.colorimetry().green().x()), p210k(colorDescription.colorimetry().green().y()),
        p210k(colorDescription.colorimetry().blue().x()), p210k(colorDescription.colorimetry().blue().y()),
        p210k(colorDescription.colorimetry().white().x()), p210k(colorDescription.colorimetry().white().y()));
    // send_complete_names(resource->handle, kwinTFToProtoTF(colorDescription.transferFunction()), ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT2020, ZCR_COLOR_MANAGER_V1_WHITEPOINT_NAMES_D65, ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT2020_NCL, ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL);
    send_done(resource->handle);
}

void ChromeColorManagementColorSpace::zcr_color_space_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ChromeColorManagementColorSpace::zcr_color_space_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

wl_resource *ChromeColorManagementColorSpace::handle() const
{
    return resource()->handle;
}

ChromeColorManagementColorSpace *ChromeColorManagementColorSpace::get(wl_resource *resource)
{
    if (const auto res = Resource::fromResource(resource)) {
        return static_cast<ChromeColorManagementColorSpace *>(res->object());
    } else {
        return nullptr;
    }
}

ChromeColorManagementColorSpaceCreator::ChromeColorManagementColorSpaceCreator(wl_client *client, uint32_t id, ChromeColorManagementColorSpace *colorspace)
    : QtWaylandServer::zcr_color_space_creator_v1(client, id, s_version)
{
    send_created(resource()->handle, colorspace->handle());
}

}

#include "moc_chromecolormanagement.cpp"
