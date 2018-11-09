/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
