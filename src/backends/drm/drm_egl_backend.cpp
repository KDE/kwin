/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
// kwin
#include "core/renderdevice.h"
#include "core/syncobjtimeline.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_layer.h"
#include "drm_virtual_output.h"
// system
#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmBackend::EglGbmBackend(RenderDevice *device)
    : EglBackend(device)
{
}

EglGbmBackend::~EglGbmBackend()
{
    cleanup();
}

bool EglGbmBackend::init()
{
    if (!initClientExtensions()) {
        qCWarning(KWIN_DRM, "Could not initialize egl");
        return false;
    }

    if (!createContext()) {
        qCWarning(KWIN_DRM, "Could not initialize rendering context");
        return false;
    }
    initWayland();
    return true;
}

} // namespace KWin

#include "moc_drm_egl_backend.cpp"
