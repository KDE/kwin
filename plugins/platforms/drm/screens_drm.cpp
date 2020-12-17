/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens_drm.h"
#include "drm_backend.h"

namespace KWin
{

DrmScreens::DrmScreens(DrmBackend *backend, QObject *parent)
    : Screens(parent)
{
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::updateCount);
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::changed);
}

}
