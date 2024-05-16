/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisplugin.h"

#include "eisbackend.h"
#include "eisinputcapturemanager.h"

#include "input.h"

EisPlugin::EisPlugin()
    : Plugin()
    , m_inputCapture(std::make_unique<KWin::EisInputCaptureManager>())
{
    KWin::input()->addInputBackend(std::make_unique<KWin::EisBackend>());
}

EisPlugin::~EisPlugin()
{
}
