/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorpipeline.h"
#include "core/colorspace.h"
#include "kwin_export.h"

#include <QMatrix4x4>
#include <QString>
#include <expected>
#include <memory>
#include <optional>

typedef void *cmsHPROFILE;

namespace KWin
{

class ColorTransformation;
class ColorLUT3D;

class KWIN_EXPORT IccProfile
{
public:
    explicit IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, std::optional<ColorPipeline> &&bToA0Tag, std::optional<ColorPipeline> &&bToA1Tag, const std::shared_ptr<ColorTransformation> &inverseEOTF, const std::shared_ptr<ColorTransformation> &vcgt, std::optional<double> minBrightness, std::optional<double> maxBrightness);
    ~IccProfile();

    /**
     * the BToA tag describes a transformation from XYZ with D50 whitepoint
     * to the display color space. May be nullptr!
     */
    const ColorPipeline *BToATag(RenderingIntent intent) const;
    /**
     * NOTE that this inverse transfer function is an estimation
     * and not necessarily exact!
     */
    std::shared_ptr<ColorTransformation> inverseTransferFunction() const;
    /**
     * The VCGT is a non-standard tag that needs to be applied before
     * pixels are sent to the display. May be nullptr!
     */
    std::shared_ptr<ColorTransformation> vcgt() const;
    const Colorimetry &colorimetry() const;
    std::optional<double> minBrightness() const;
    std::optional<double> maxBrightness() const;

    static std::expected<std::unique_ptr<IccProfile>, QString> load(const QString &path);
    static const ColorDescription s_connectionSpace;

private:
    cmsHPROFILE const m_handle;
    const Colorimetry m_colorimetry;
    const std::optional<ColorPipeline> m_bToA0Tag;
    const std::optional<ColorPipeline> m_bToA1Tag;
    const std::shared_ptr<ColorTransformation> m_inverseEOTF;
    const std::shared_ptr<ColorTransformation> m_vcgt;
    const std::optional<double> m_minBrightness;
    const std::optional<double> m_maxBrightness;
};

}
