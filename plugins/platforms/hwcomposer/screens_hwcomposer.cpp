/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "screens_hwcomposer.h"
#include "hwcomposer_backend.h"

namespace KWin
{

HwcomposerScreens::HwcomposerScreens(HwcomposerBackend *backend, QObject *parent)
    : Screens(parent)
{
    connect(backend, &HwcomposerBackend::screensQueried, this, &HwcomposerScreens::updateCount);
    connect(backend, &HwcomposerBackend::screensQueried, this, &HwcomposerScreens::changed);
}

}
