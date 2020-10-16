/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2003 Karol Szwed <kszwed@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_GEOMETRY_TIP_H
#define KWIN_GEOMETRY_TIP_H
#include "xcbutils.h"

#include <QLabel>

namespace KWin
{

class GeometryTip: public QLabel
{
    Q_OBJECT
public:
    GeometryTip(const Xcb::GeometryHints* xSizeHints);
    ~GeometryTip() override;
    void setGeometry(const QRect& geom);

private:
    const Xcb::GeometryHints* sizeHints;
};

} // namespace

#endif
