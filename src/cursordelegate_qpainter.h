/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cursordelegate.h"

namespace KWin
{

class Output;

class CursorDelegateQPainter final : public CursorDelegate
{
    Q_OBJECT

public:
    explicit CursorDelegateQPainter(Output *output, QObject *parent = nullptr);

    void paint(RenderTarget *renderTarget, const QRegion &region) override;
};

} // namespace KWin
