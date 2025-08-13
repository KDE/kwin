/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fakeoutput.h"

FakeBackendOutput::FakeBackendOutput()
{
    setMode(QSize(1024, 720), 60000);
}

bool FakeBackendOutput::testPresentation(const std::shared_ptr<KWin::OutputFrame> &frame)
{
    return false;
}

bool FakeBackendOutput::present(const QList<KWin::OutputLayer *> &layersToUpdate, const std::shared_ptr<KWin::OutputFrame> &frame)
{
    return false;
}

KWin::RenderLoop *FakeBackendOutput::renderLoop() const
{
    return nullptr;
}

void FakeBackendOutput::setMode(QSize size, uint32_t refreshRate)
{
    auto mode = std::make_shared<KWin::OutputMode>(size, refreshRate);

    State state = m_state;
    state.modes = {mode};
    state.currentMode = mode;
    setState(state);
}

void FakeBackendOutput::setTransform(KWin::OutputTransform transform)
{
    State state = m_state;
    state.transform = transform;
    setState(state);
}

void FakeBackendOutput::moveTo(const QPoint &pos)
{
    State state = m_state;
    state.position = pos;
    setState(state);
}

void FakeBackendOutput::setScale(qreal scale)
{
    State state = m_state;
    state.scale = scale;
    setState(state);
}

void FakeBackendOutput::setSubPixel(SubPixel subPixel)
{
    setInformation({
        .subPixel = subPixel,
    });
}

void FakeBackendOutput::setDpmsSupported(bool supported)
{
    setInformation({
        .capabilities = supported ? Capability::Dpms : Capabilities(),
    });
}

void FakeBackendOutput::setPhysicalSize(QSize size)
{
    setInformation({
        .physicalSize = size,
    });
}

void FakeBackendOutput::setName(const QString &name)
{
    Information info = m_information;
    info.name = name;
    setInformation(info);
}

void FakeBackendOutput::setManufacturer(const QString &manufacturer)
{
    Information info = m_information;
    info.manufacturer = manufacturer;
    setInformation(info);
}

void FakeBackendOutput::setModel(const QString &model)
{
    Information info = m_information;
    info.model = model;
    setInformation(info);
}

#include "moc_fakeoutput.cpp"
