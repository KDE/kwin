/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2003 Karol Szwed <kszwed@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "geometrytip.h"

namespace KWin
{

GeometryTip::GeometryTip(const Xcb::GeometryHints* xSizeHints):
    QLabel(nullptr)
{
    setObjectName(QLatin1String("kwingeometry"));
    setMargin(1);
    setIndent(0);
    setLineWidth(1);
    setFrameStyle(QFrame::Raised | QFrame::StyledPanel);
    setAlignment(Qt::AlignCenter | Qt::AlignTop);
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    sizeHints = xSizeHints;
}

GeometryTip::~GeometryTip()
{
}

static QString numberWithSign(int n)
{
    const QLocale locale;
    const QChar sign = n >= 0 ? locale.positiveSign() : locale.negativeSign();
    return sign + QString::number(std::abs(n));
}

void GeometryTip::setGeometry(const QRect& geom)
{
    int w = geom.width();
    int h = geom.height();

    if (sizeHints) {
        if (sizeHints->hasResizeIncrements()) {
            w = (w - sizeHints->baseSize().width()) / sizeHints->resizeIncrements().width();
            h = (h - sizeHints->baseSize().height()) / sizeHints->resizeIncrements().height();
        }
    }

    h = qMax(h, 0);   // in case of isShade() and PBaseSize
    const QString pos = QStringLiteral("%1,%2<br>(<b>%3&nbsp;x&nbsp;%4</b>)")
        .arg(numberWithSign(geom.x()))
        .arg(numberWithSign(geom.y()))
        .arg(w)
        .arg(h);
    setText(pos);
    adjustSize();
    move(geom.x() + ((geom.width()  - width())  / 2),
         geom.y() + ((geom.height() - height()) / 2));
}

} // namespace

