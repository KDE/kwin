/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVariantList>

namespace Aurorae
{

/*
 * KDecoration2::BorderSize doesn't map to the indices used for the Aurorae SVG Button Sizes.
 * BorderSize defines None and NoSideBorder as index 0 and 1. These do not make sense for Button
 * Size, thus we need to perform a mapping between the enum value and the config value.
 */
static const int s_indexMapper = 2;

static QString findTheme(const QVariantList &args)
{
    if (args.isEmpty()) {
        return QString();
    }
    const auto map = args.first().toMap();
    auto it = map.constFind(QStringLiteral("theme"));
    if (it == map.constEnd()) {
        return QString();
    }
    return it.value().toString();
}

}
