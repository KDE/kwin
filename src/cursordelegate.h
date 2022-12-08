/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/renderlayerdelegate.h"

namespace KWin
{

class Output;

class CursorDelegate : public RenderLayerDelegate
{
    Q_OBJECT

public:
    explicit CursorDelegate(Output *output, QObject *parent = nullptr);

    void postPaint() override;

private:
    Output *m_output;
};

} // namespace KWin
