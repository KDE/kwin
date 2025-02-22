/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "edid.h"

#include "config-kwin.h"

#include "c_ptr.h"
#include "common.h"

#include <QFile>
#include <QStandardPaths>
#include <cstdlib>

#include <KLocalizedString>
#include <QCryptographicHash>

extern "C" {
#include <libdisplay-info/cta.h>
#include <libdisplay-info/displayid.h>
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
}

namespace KWin
{

static QByteArray parsePnpId(const uint8_t *data)
{
    // Decode PNP ID from three 5 bit words packed into 2 bytes:
    //
    // | Byte |        Bit                    |
    // |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    // ----------------------------------------
    // |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
    // |      | * |    Character 1    | Char 2|
    // ----------------------------------------
    // |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
    // |      | Character2|      Character 3  |
    // ----------------------------------------
    const uint offset = 0x8;

    char pnpId[4];
    pnpId[0] = 'A' + ((data[offset + 0] >> 2) & 0x1f) - 1;
    pnpId[1] = 'A' + (((data[offset + 0] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    pnpId[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    pnpId[3] = '\0';

    return QByteArray(pnpId);
}

static QByteArray parseEisaId(const uint8_t *data)
{
    for (int i = 72; i <= 108; i += 18) {
        // Skip the block if it isn't used as monitor descriptor.
        if (data[i]) {
            continue;
        }
        if (data[i + 1]) {
            continue;
        }

        // We have found the EISA ID, it's stored as ASCII.
        if (data[i + 3] == 0xfe) {
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 13).trimmed();
        }
    }

    // If there isn't an ASCII EISA ID descriptor, try to decode PNP ID
    return parsePnpId(data);
}

static QByteArray parseVendor(const uint8_t *data)
{
    const auto pnpId = parsePnpId(data);

    // Map to vendor name
    QFile pnpFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("hwdata/pnp.ids")));
    if (pnpFile.exists() && pnpFile.open(QIODevice::ReadOnly)) {
        while (!pnpFile.atEnd()) {
            const auto line = pnpFile.readLine();
            if (line.startsWith(pnpId)) {
                return line.mid(4).trimmed();
            }
        }
    }

    return {};
}

static QSize determineScreenPhysicalSizeMm(const di_edid *edid)
{
    // An EDID can contain zero or more detailed timing definitions, which can
    // contain more precise physical dimensions (in millimeters, as opposed to
    // centimeters). Pick the first sane physical dimension from detailed timings
    // and fall back to the basic dimensions.
    const struct di_edid_detailed_timing_def *const *detailedTimings = di_edid_get_detailed_timing_defs(edid);
    // detailedTimings is a null-terminated array.
    for (int i = 0; detailedTimings[i] != nullptr; i++) {
        const struct di_edid_detailed_timing_def *timing = detailedTimings[i];
        // Sanity check dimensions: physical aspect ratio should roughly equal
        // mode aspect ratio (i.e. width_in_pixels / height_in_pixels).
        // This assumes that the display has square pixels, but this is true for
        // basically all modern displays.
        if (timing->horiz_image_mm > 0 && timing->vert_image_mm > 0
            && timing->horiz_video > 0 && timing->vert_video > 0) {
            const double physicalAspectRatio = double(timing->horiz_image_mm) / double(timing->vert_image_mm);
            const double modeAspectRatio = double(timing->horiz_video) / double(timing->vert_video);

            if (std::abs(physicalAspectRatio - modeAspectRatio) <= 0.1) {
                return QSize(timing->horiz_image_mm, timing->vert_image_mm);
            }
        }
    }
    const di_edid_screen_size *screenSize = di_edid_get_screen_size(edid);
    return QSize(screenSize->width_cm, screenSize->height_cm) * 10;
}

Edid::Edid()
{
}

Edid::Edid(const void *data, uint32_t size)
    : Edid(QByteArrayView(reinterpret_cast<const uint8_t *>(data), size))
{
}

Edid::Edid(QByteArrayView data, std::optional<QByteArrayView> identifierOverride)
    : Edid(data)
{
    if (identifierOverride.has_value()) {
        m_identifier = identifierOverride->toByteArray();
    }
}

Edid::Edid(QByteArrayView data)
{
    m_raw = QByteArray(data.data(), data.size());
    if (m_raw.isEmpty()) {
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(m_raw);
    m_hash = QString::fromLatin1(hash.result().toHex());

    auto info = di_info_parse_edid(data.data(), data.size());
    if (!info) {
        qCWarning(KWIN_CORE, "parsing edid failed");
        return;
    }
    const di_edid *edid = di_info_get_edid(info);
    const di_edid_vendor_product *productInfo = di_edid_get_vendor_product(edid);

    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data.data());

    // basic output information
    m_physicalSize = determineScreenPhysicalSizeMm(edid);
    m_eisaId = parseEisaId(bytes);
    UniqueCPtr<char> monitorName{di_info_get_model(info)};
    m_monitorName = QByteArray(monitorName.get());
    UniqueCPtr<char> serial{di_info_get_serial(info)};
    m_serialNumber = QByteArray(serial.get());
    m_vendor = parseVendor(bytes);

    m_identifier = QByteArray(productInfo->manufacturer, 3) + " " + QByteArray::number(productInfo->product) + " " + QByteArray::number(productInfo->serial) + " "
        + QByteArray::number(productInfo->manufacture_week) + " " + QByteArray::number(productInfo->manufacture_year) + " " + QByteArray::number(productInfo->model_year);

    // colorimetry and HDR metadata
    const auto chromaticity = di_info_get_default_color_primaries(info);
    if (chromaticity->has_primaries && chromaticity->has_default_white_point) {
        const xy red{chromaticity->primary[0].x, chromaticity->primary[0].y};
        const xy green{chromaticity->primary[1].x, chromaticity->primary[1].y};
        const xy blue{chromaticity->primary[2].x, chromaticity->primary[2].y};
        const xy white{chromaticity->default_white.x, chromaticity->default_white.y};
        if (Colorimetry::isReal(red, green, blue, white)) {
            m_colorimetry = Colorimetry{
                red,
                green,
                blue,
                white,
            };
        } else {
            qCWarning(KWIN_CORE) << "EDID colorimetry" << red << green << blue << white << "is invalid";
        }
    } else {
        m_colorimetry.reset();
    }

    const auto metadata = di_info_get_hdr_static_metadata(info);
    const auto colorimetry = di_info_get_supported_signal_colorimetry(info);
    m_hdrMetadata = HDRMetadata{
        .desiredContentMinLuminance = metadata->desired_content_min_luminance,
        .desiredContentMaxLuminance = metadata->desired_content_max_luminance > 0 ? std::make_optional(metadata->desired_content_max_luminance) : std::nullopt,
        .desiredMaxFrameAverageLuminance = metadata->desired_content_max_frame_avg_luminance > 0 ? std::make_optional(metadata->desired_content_max_frame_avg_luminance) : std::nullopt,
        .supportsPQ = metadata->pq,
        .supportsBT2020 = colorimetry->bt2020_rgb || colorimetry->bt2020_ycc || colorimetry->bt2020_cycc,
    };

    const di_displayid *displayid = nullptr;
    const di_edid_ext *const *exts = di_edid_get_extensions(edid);
    for (; *exts != nullptr; exts++) {
        if (!displayid && (displayid = di_edid_ext_get_displayid(*exts))) {
            continue;
        }
    }
    if (displayid) {
        const di_displayid_display_params *params = nullptr;
        const di_displayid_type_i_ii_vii_timing *const *type1Timings = nullptr;
        const di_displayid_type_i_ii_vii_timing *const *type2Timings = nullptr;
        for (auto block = di_displayid_get_data_blocks(displayid); *block != nullptr; block++) {
            if (!params && (params = di_displayid_data_block_get_display_params(*block))) {
                continue;
            }
            if (!type1Timings && (type1Timings = di_displayid_data_block_get_type_i_timings(*block))) {
                continue;
            }
            if (!type2Timings && (type2Timings = di_displayid_data_block_get_type_ii_timings(*block))) {
                continue;
            }
        }
        if (params && params->horiz_pixels != 0 && params->vert_pixels != 0) {
            m_nativeResolution = QSize(params->horiz_pixels, params->vert_pixels);
        }
        if (type1Timings && !m_nativeResolution) {
            for (auto timing = type1Timings; *timing != nullptr; timing++) {
                if ((*timing)->preferred && (!m_nativeResolution || m_nativeResolution->width() < (*timing)->horiz_active || m_nativeResolution->height() < (*timing)->vert_active)) {
                    m_nativeResolution = QSize((*timing)->horiz_active, (*timing)->vert_active);
                }
            }
        }
        if (type2Timings && !m_nativeResolution) {
            for (auto timing = type2Timings; *timing != nullptr; timing++) {
                if ((*timing)->preferred && (!m_nativeResolution || m_nativeResolution->width() < (*timing)->horiz_active || m_nativeResolution->height() < (*timing)->vert_active)) {
                    m_nativeResolution = QSize((*timing)->horiz_active, (*timing)->vert_active);
                }
            }
        }
    }
    // EDID often contains misleading information for backwards compatibility
    // so only use it if we don't have the same info from DisplayID
    if (const auto misc = di_edid_get_misc_features(edid); misc && !m_nativeResolution) {
        if (misc->preferred_timing_is_native) {
            const auto timing = di_edid_get_detailed_timing_defs(edid);
            if (*timing != nullptr) {
                m_nativeResolution = QSize((*timing)->horiz_video, (*timing)->vert_video);
            }
        }
    }

    m_isValid = true;
    di_info_destroy(info);
}

std::optional<QSize> Edid::likelyNativeResolution() const
{
    return m_nativeResolution;
}

bool Edid::isValid() const
{
    return m_isValid;
}

QSize Edid::physicalSize() const
{
    return m_physicalSize;
}

QByteArray Edid::eisaId() const
{
    return m_eisaId;
}

QByteArray Edid::monitorName() const
{
    return m_monitorName;
}

QByteArray Edid::serialNumber() const
{
    return m_serialNumber;
}

QByteArray Edid::vendor() const
{
    return m_vendor;
}

QByteArray Edid::raw() const
{
    return m_raw;
}

QString Edid::manufacturerString() const
{
    QString manufacturer;
    if (!m_vendor.isEmpty()) {
        manufacturer = QString::fromLatin1(m_vendor);
    } else if (!m_eisaId.isEmpty()) {
        manufacturer = QString::fromLatin1(m_eisaId);
    }
    return manufacturer;
}

QString Edid::nameString() const
{
    if (!m_monitorName.isEmpty()) {
        QString m = QString::fromLatin1(m_monitorName);
        if (!m_serialNumber.isEmpty()) {
            m.append('/');
            m.append(QString::fromLatin1(m_serialNumber));
        }
        return m;
    } else if (!m_serialNumber.isEmpty()) {
        return QString::fromLatin1(m_serialNumber);
    } else {
        return i18n("unknown");
    }
}

QString Edid::hash() const
{
    return m_hash;
}

std::optional<Colorimetry> Edid::colorimetry() const
{
    return m_colorimetry;
}

double Edid::desiredMinLuminance() const
{
    return m_hdrMetadata ? m_hdrMetadata->desiredContentMinLuminance : 0;
}

std::optional<double> Edid::desiredMaxFrameAverageLuminance() const
{
    return m_hdrMetadata ? m_hdrMetadata->desiredMaxFrameAverageLuminance : std::nullopt;
}

std::optional<double> Edid::desiredMaxLuminance() const
{
    return m_hdrMetadata ? m_hdrMetadata->desiredContentMaxLuminance : std::nullopt;
}

bool Edid::supportsPQ() const
{
    return m_hdrMetadata && m_hdrMetadata->supportsPQ;
}

bool Edid::supportsBT2020() const
{
    return m_hdrMetadata && m_hdrMetadata->supportsBT2020;
}

QByteArray Edid::identifier() const
{
    return m_identifier;
}

} // namespace KWin
