/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fakeoutput.h"

FakeOutput::FakeOutput()
{
    setMode(QSize(1024, 720), 60000);
}

KWin::RenderLoop *FakeOutput::renderLoop() const
{
    return nullptr;
}

void FakeOutput::setMode(QSize size, uint32_t refreshRate)
{
    auto mode = std::make_shared<KWin::OutputMode>(size, refreshRate);
    setModesInternal({mode}, mode);
}

void FakeOutput::setSubPixel(SubPixel subPixel)
{
    setInformation({
        .subPixel = subPixel,
    });
}

void FakeOutput::setDpmsSupported(bool supported)
{
    setInformation({
        .capabilities = supported ? Capability::Dpms : Capabilities(),
    });
}

void FakeOutput::setPhysicalSize(QSize size)
{
    setInformation({
        .physicalSize = size,
    });
}

void FakeOutput::setTransform(Transform transform)
{
    setTransformInternal(transform);
}
