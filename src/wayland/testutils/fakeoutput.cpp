/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fakeoutput.h"

namespace KWin
{

Output::Output(QObject *parent)
    : QObject(parent)
{
}

Output::~Output() = default;

void Output::setInformation(const Information &info)
{
    m_information = info;
}

void Output::setTransformInternal(Transform transform)
{
    m_transform = transform;
}

void Output::setDpmsModeInternal(DpmsMode dpmsMode)
{
    m_dpmsMode = dpmsMode;
}

void Output::setModesInternal(const QList<QSharedPointer<OutputMode>> &modes, const QSharedPointer<OutputMode> &currentMode)
{
    m_modes = modes;
    m_currentMode = currentMode;
}

OutputMode::OutputMode(const QSize &size, uint32_t refreshRate, Flags flags)
    : m_size(size)
    , m_refreshRate(refreshRate)
    , m_flags(flags)
{
}

QSize OutputMode::size() const
{
    return m_size;
}

uint32_t OutputMode::refreshRate() const
{
    return m_refreshRate;
}

OutputMode::Flags OutputMode::flags() const
{
    return m_flags;
}
}

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
    QSharedPointer<KWin::OutputMode> mode = QSharedPointer<KWin::OutputMode>::create(size, refreshRate);
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

void FakeOutput::setDpmsMode(DpmsMode mode)
{
    Q_EMIT dpmsModeRequested(mode);
    setDpmsModeInternal(mode);
}
