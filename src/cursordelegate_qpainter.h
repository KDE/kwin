/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/renderlayerdelegate.h"

#include <QImage>

namespace KWin
{

class CursorDelegateQPainter final : public RenderLayerDelegate
{
public:
    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    QImage m_buffer;
};

} // namespace KWin
