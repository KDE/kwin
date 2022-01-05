/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colors.h"

#include <lcms2.h>

namespace KWin
{

ColorTransformation::ColorTransformation(cmsPipeline *pipeline)
    : m_pipeline(pipeline)
{
}

ColorTransformation::~ColorTransformation()
{
    cmsStage *last = nullptr;
    do {
        cmsPipelineUnlinkStage(m_pipeline, cmsAT_END, &last);
    } while (last);
    cmsPipelineFree(m_pipeline);
}

std::tuple<uint16_t, uint16_t, uint16_t> ColorTransformation::transform(uint16_t r, uint16_t g, uint16_t b) const
{
    const uint16_t in[3] = {r, g, b};
    uint16_t out[3] = {0, 0, 0};
    cmsPipelineEval16(in, out, m_pipeline);
    return {out[0], out[1], out[2]};
}

ColorLUT::ColorLUT(const QSharedPointer<ColorTransformation> &transformation, size_t size)
    : m_transformation(transformation)
{
    m_data.fill(0, 3 * size);
    for (uint64_t i = 0; i < size; i++) {
        const uint16_t index = (i * 0xFFFF) / size;
        std::tie(m_data[i], m_data[size + i], m_data[size * 2 + i]) = transformation->transform(index, index, index);
    }
}

uint16_t *ColorLUT::red() const
{
    return const_cast<uint16_t *>(m_data.constData());
}

uint16_t *ColorLUT::green() const
{
    return const_cast<uint16_t *>(m_data.constData() + size());
}

uint16_t *ColorLUT::blue() const
{
    return const_cast<uint16_t *>(m_data.constData() + 2 * size());
}

size_t ColorLUT::size() const
{
    return m_data.size() / 3;
}

QSharedPointer<ColorTransformation> ColorLUT::transformation() const
{
    return m_transformation;
}

}
