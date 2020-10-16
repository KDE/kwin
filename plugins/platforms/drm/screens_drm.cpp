/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens_drm.h"
#include "drm_backend.h"
#include "drm_output.h"

namespace KWin
{

DrmScreens::DrmScreens(DrmBackend *backend, QObject *parent)
    : OutputScreens(backend, parent)
    , m_backend(backend)
{
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::updateCount);
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::changed);
}

DrmScreens::~DrmScreens() = default;

bool DrmScreens::supportsTransformations(int screen) const
{
    const auto enOuts = m_backend->drmEnabledOutputs();
    if (screen >= enOuts.size()) {
        return false;
    }
    return enOuts.at(screen)->supportsTransformations();
}

}
