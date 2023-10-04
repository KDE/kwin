/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "iccprofile.h"
#include "colorlut.h"
#include "colorpipelinestage.h"
#include "colortransformation.h"
#include "utils/common.h"

#include <lcms2.h>

namespace KWin
{

IccProfile::IccProfile(cmsHPROFILE handle, const std::shared_ptr<ColorTransformation> &vcgt)
    : m_handle(handle)
    , m_vcgt(vcgt)
{
}

IccProfile::~IccProfile()
{
    cmsCloseProfile(m_handle);
}

std::shared_ptr<ColorTransformation> IccProfile::vcgt() const
{
    return m_vcgt;
}

std::unique_ptr<IccProfile> IccProfile::load(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    cmsHPROFILE handle = cmsOpenProfileFromFile(path.toUtf8(), "r");
    if (!handle) {
        qCWarning(KWIN_CORE) << "Failed to open color profile file:" << path;
        return nullptr;
    }
    if (cmsGetDeviceClass(handle) != cmsSigDisplayClass) {
        qCWarning(KWIN_CORE) << "Only Display ICC profiles are supported";
        return nullptr;
    }
    if (cmsGetPCS(handle) != cmsColorSpaceSignature::cmsSigXYZData) {
        qCWarning(KWIN_CORE) << "Only ICC profiles with a XYZ connection space are supported";
        return nullptr;
    }
    if (cmsGetColorSpace(handle) != cmsColorSpaceSignature::cmsSigRgbData) {
        qCWarning(KWIN_CORE) << "Only ICC profiles with RGB color spaces are supported";
        return nullptr;
    }

    std::shared_ptr<ColorTransformation> vcgt;
    cmsToneCurve **vcgtTag = static_cast<cmsToneCurve **>(cmsReadTag(handle, cmsSigVcgtTag));
    if (!vcgtTag || !vcgtTag[0]) {
        qCWarning(KWIN_CORE) << "Profile" << path << "has no VCGT tag";
    } else {
        // Need to duplicate the VCGT tone curves as they are owned by the profile.
        cmsToneCurve *toneCurves[] = {
            cmsDupToneCurve(vcgtTag[0]),
            cmsDupToneCurve(vcgtTag[1]),
            cmsDupToneCurve(vcgtTag[2]),
        };
        std::vector<std::unique_ptr<ColorPipelineStage>> stages;
        stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, 3, toneCurves)));
        vcgt = std::make_shared<ColorTransformation>(std::move(stages));
    }

    return std::make_unique<IccProfile>(handle, vcgt);
}

}
