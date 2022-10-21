/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <stdint.h>
#include <tuple>
#include <vector>

#include "kwin_export.h"

typedef struct _cmsPipeline_struct cmsPipeline;

namespace KWin
{

class ColorPipelineStage;

class KWIN_EXPORT ColorTransformation
{
public:
    ColorTransformation(std::vector<std::unique_ptr<ColorPipelineStage>> &&stages);
    ~ColorTransformation();

    bool valid() const;

    std::tuple<uint16_t, uint16_t, uint16_t> transform(uint16_t r, uint16_t g, uint16_t b) const;

private:
    cmsPipeline *const m_pipeline;
    const std::vector<std::unique_ptr<ColorPipelineStage>> m_stages;
    bool m_valid = true;
};

}
