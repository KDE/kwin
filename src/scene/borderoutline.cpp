/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "scene/borderoutline.h"

#include <KDecoration3/Decoration>

namespace KWin
{

BorderOutline BorderOutline::from(const KDecoration3::BorderOutline &outline)
{
    return BorderOutline(outline.thickness(), outline.color(), BorderRadius::from(outline.radius()));
}

} // namespace KWin
