/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colortransformation.h"
#include "colorpipelinestage.h"

#include <lcms2.h>

#include "utils/common.h"

namespace KWin
{

ColorTransformation::ColorTransformation(std::vector<std::unique_ptr<ColorPipelineStage>> &&stages)
    : m_pipeline(cmsPipelineAlloc(nullptr, 3, 3))
    , m_stages(std::move(stages))
{
    if (!m_pipeline) {
        qCWarning(KWIN_CORE) << "Failed to allocate cmsPipeline!";
        m_valid = false;
        return;
    }
    for (auto &stage : m_stages) {
        if (!cmsPipelineInsertStage(m_pipeline, cmsAT_END, stage->stage())) {
            qCWarning(KWIN_CORE) << "Failed to insert cmsPipeline stage!";
            m_valid = false;
            return;
        }
    }
}

ColorTransformation::~ColorTransformation()
{
    if (m_pipeline) {
        cmsStage *last = nullptr;
        do {
            cmsPipelineUnlinkStage(m_pipeline, cmsAT_END, &last);
        } while (last);
        cmsPipelineFree(m_pipeline);
    }
}

void ColorTransformation::append(ColorTransformation *transformation)
{
    for (auto &stage : transformation->m_stages) {
        auto dup = stage->dup();
        if (!cmsPipelineInsertStage(m_pipeline, cmsAT_END, dup->stage())) {
            qCWarning(KWIN_CORE) << "Failed to insert cmsPipeline stage!";
            m_valid = false;
            return;
        }
        m_stages.push_back(std::move(dup));
    }
}

bool ColorTransformation::valid() const
{
    return m_valid;
}

std::tuple<uint16_t, uint16_t, uint16_t> ColorTransformation::transform(uint16_t r, uint16_t g, uint16_t b) const
{
    const uint16_t in[3] = {r, g, b};
    uint16_t out[3] = {0, 0, 0};
    cmsPipelineEval16(in, out, m_pipeline);
    return {out[0], out[1], out[2]};
}

QVector3D ColorTransformation::transform(QVector3D in) const
{
    QVector3D ret;
    cmsPipelineEvalFloat(&in[0], &ret[0], m_pipeline);
    return ret;
}

std::unique_ptr<ColorTransformation> ColorTransformation::createScalingTransform(const QVector3D &scale)
{
    std::array<double, 3> curveParams = {1.0, scale.x(), 0.0};
    auto r = cmsBuildParametricToneCurve(nullptr, 2, curveParams.data());
    curveParams = {1.0, scale.y(), 0.0};
    auto g = cmsBuildParametricToneCurve(nullptr, 2, curveParams.data());
    curveParams = {1.0, scale.z(), 0.0};
    auto b = cmsBuildParametricToneCurve(nullptr, 2, curveParams.data());
    if (!r || !g || !b) {
        qCWarning(KWIN_CORE) << "Failed to build tone curves";
        return nullptr;
    }
    const std::array curves = {r, g, b};
    const auto stage = cmsStageAllocToneCurves(nullptr, 3, curves.data());
    if (!stage) {
        qCWarning(KWIN_CORE) << "Failed to allocate tone curves";
        return nullptr;
    }
    std::vector<std::unique_ptr<ColorPipelineStage>> stages;
    stages.push_back(std::make_unique<ColorPipelineStage>(stage));
    auto transform = std::make_unique<ColorTransformation>(std::move(stages));
    if (!transform->valid()) {
        return nullptr;
    }
    return transform;
}
}
