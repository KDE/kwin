/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpipelinestage.h"

#include <lcms2.h>

#include "utils/common.h"

namespace KWin
{

ColorPipelineStage::ColorPipelineStage(cmsStage *stage)
    : m_stage(stage)
{
}

ColorPipelineStage::~ColorPipelineStage()
{
    if (m_stage) {
        cmsStageFree(m_stage);
    }
}

std::unique_ptr<ColorPipelineStage> ColorPipelineStage::dup() const
{
    if (m_stage) {
        auto dup = cmsStageDup(m_stage);
        if (dup) {
            return std::make_unique<ColorPipelineStage>(dup);
        } else {
            qCWarning(KWIN_CORE) << "Failed to duplicate cmsStage!";
        }
    }
    return nullptr;
}

cmsStage *ColorPipelineStage::stage() const
{
    return m_stage;
}

}
