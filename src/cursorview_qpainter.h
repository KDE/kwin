/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cursorview.h"

namespace KWin
{

class QPainterCursorView final : public CursorView
{
    Q_OBJECT

public:
    explicit QPainterCursorView(QObject *parent = nullptr);

    void paint(AbstractOutput *output, const QRegion &region) override;
};

} // namespace KWin
