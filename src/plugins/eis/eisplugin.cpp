/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisplugin.h"

#include "eisbackend.h"

#include "input.h"

EisPlugin::EisPlugin()
    : Plugin()
{
    KWin::input()->addInputBackend(std::make_unique<KWin::EisBackend>());
}

EisPlugin::~EisPlugin()
{
}
