/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/renderlayerdelegate.h"

namespace KWin
{

class Output;

class CursorDelegateVulkan : public RenderLayerDelegate
{
public:
    CursorDelegateVulkan(Output *output);
    ~CursorDelegateVulkan() override;

    void paint(const RenderTarget &renderTarget, const QRegion &region) override;

private:
    Output *const m_output;
};

}
