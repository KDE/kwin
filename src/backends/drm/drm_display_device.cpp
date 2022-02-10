/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_display_device.h"

namespace KWin
{

DrmDisplayDevice::DrmDisplayDevice(DrmGpu *gpu)
    : m_gpu(gpu)
{
}

DrmGpu *DrmDisplayDevice::gpu() const
{
    return m_gpu;
}

}
