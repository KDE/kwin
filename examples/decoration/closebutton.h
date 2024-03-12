/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KDecoration2/DecorationButton>

class BasicCloseButton : public KDecoration2::DecorationButton
{
    Q_OBJECT

public:
    BasicCloseButton(KDecoration2::Decoration *decoration, QObject *parent = nullptr);

    void paint(QPainter *painter, const QRect &repaintRegion) override;

private:
    void resize();
};
