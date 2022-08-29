/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <memory>

typedef struct _cmsStage_struct cmsStage;

namespace KWin
{

class KWIN_EXPORT ColorPipelineStage
{
public:
    ColorPipelineStage(cmsStage *stage);
    ~ColorPipelineStage();

    std::unique_ptr<ColorPipelineStage> dup() const;
    cmsStage *stage() const;

private:
    cmsStage *const m_stage;
};

}
