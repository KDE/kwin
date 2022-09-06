/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"

namespace KWin
{

class PlaceholderOutput : public Output
{
    Q_OBJECT

public:
    PlaceholderOutput(const QSize &size, qreal scale = 1);
    ~PlaceholderOutput() override;

    RenderLoop *renderLoop() const override;

private:
    std::unique_ptr<RenderLoop> m_renderLoop;
};

} // namespace KWin
