/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "renderlayerdelegate.h"

namespace KWin
{

class CursorDelegateQPainter final : public RenderLayerDelegate
{
    Q_OBJECT

public:
    explicit CursorDelegateQPainter(QObject *parent = nullptr);

    void paint(RenderTarget *renderTarget, const QRegion &region) override;
};

} // namespace KWin
