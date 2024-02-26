/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorspace.h"
#include "kwin_export.h"

#include <QMatrix4x4>
#include <QString>
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
    struct BToATagData
    {
        std::unique_ptr<ColorTransformation> B;
        std::optional<QMatrix4x4> matrix;
        std::unique_ptr<ColorTransformation> M;
        std::unique_ptr<ColorLUT3D> CLut;
        std::unique_ptr<ColorTransformation> A;
    };

    explicit IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, BToATagData &&bToATag, const std::shared_ptr<ColorTransformation> &vcgt, std::optional<double> brightness);
    explicit IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, const std::shared_ptr<ColorTransformation> &inverseEOTF, const std::shared_ptr<ColorTransformation> &vcgt, std::optional<double> brightness);
    ~IccProfile();

    /**
     * the BToA tag describes a transformation from XYZ with D50 whitepoint
     * to the display color space. May be nullptr!
     */
    const BToATagData *BtToATag() const;
    /**
     * Contains the inverse of the TRC tags. May be nullptr!
     */
    std::shared_ptr<ColorTransformation> inverseEOTF() const;
    /**
     * The VCGT is a non-standard tag that needs to be applied before
     * pixels are sent to the display. May be nullptr!
     */
    std::shared_ptr<ColorTransformation> vcgt() const;
    const Colorimetry &colorimetry() const;
    /**
     * The brightness with a completely white output, in nits
     */
    std::optional<double> brightness() const;

    static std::unique_ptr<IccProfile> load(const QString &path);

private:
    cmsHPROFILE const m_handle;
    const Colorimetry m_colorimetry;
    const std::optional<BToATagData> m_bToATag;
    const std::shared_ptr<ColorTransformation> m_inverseEOTF;
    const std::shared_ptr<ColorTransformation> m_vcgt;
    const std::optional<double> m_brightness;
};

}
