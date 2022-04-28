/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <qnamespace.h>

namespace KWin
{

uint32_t qtMouseButtonToButton(Qt::MouseButton button);
Qt::MouseButton buttonToQtMouseButton(uint32_t button);

}
