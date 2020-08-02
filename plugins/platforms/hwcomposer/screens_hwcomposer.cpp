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
    : OutputScreens(backend, parent)
    , m_backend(backend)
{
    connect(m_backend, &HwcomposerBackend::screensQueried, this, &OutputScreens::updateCount);
    connect(m_backend, &HwcomposerBackend::screensQueried, this, &OutputScreens::changed);
}

}
