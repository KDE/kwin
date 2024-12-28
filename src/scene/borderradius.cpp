/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "scene/borderradius.h"

#include <KDecoration3/Decoration>

namespace KWin
{

BorderRadius BorderRadius::from(const KDecoration3::BorderRadius &radius)
{
    return BorderRadius(radius.topLeft(), radius.topRight(), radius.bottomRight(), radius.bottomLeft());
}

} // namespace KWin
