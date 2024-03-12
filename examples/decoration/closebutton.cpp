/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "closebutton.h"
#include "decoration.h"

#include <QPainter>

BasicCloseButton::BasicCloseButton(KDecoration2::Decoration *decoration, QObject *parent)
    : KDecoration2::DecorationButton(KDecoration2::DecorationButtonType::Close, decoration, parent)
{
    connect(this, &BasicCloseButton::hoveredChanged, this, [this] {
        update();
    });

    resize();
    connect(decoration, &KDecoration2::Decoration::titleBarChanged, this, &BasicCloseButton::resize);
}

void BasicCloseButton::resize()
{
    const int size = std::min(decoration()->titleBar().width(), decoration()->titleBar().height());
    setGeometry(QRect(0, 0, size, size));
}

void BasicCloseButton::paint(QPainter *painter, const QRect &repaintRegion)
{
    const QRectF buttonRect = geometry();
    QRectF crossRect = QRectF(0, 0, 10, 10);
    crossRect.moveCenter(buttonRect.center().toPoint());

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing, false);

    // Background.
    painter->setPen(Qt::NoPen);
    painter->setBrush(isHovered() ? Qt::red : Qt::transparent);
    painter->drawRect(buttonRect);

    // Foreground.
    painter->setPen(Qt::white);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(crossRect.topLeft(), crossRect.bottomRight());
    painter->drawLine(crossRect.topRight(), crossRect.bottomLeft());

    painter->restore();
}
