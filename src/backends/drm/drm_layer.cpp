/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"

namespace KWin
{

DrmLayer::DrmLayer(KWin::DrmDisplayDevice* device)
    : m_displayDevice(device)
{
}

DrmLayer::~DrmLayer() = default;

DrmDisplayDevice *DrmLayer::displayDevice() const
{
    return m_displayDevice;
}

}

#include "drm_layer.moc"
