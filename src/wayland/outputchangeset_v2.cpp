/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "outputchangeset_v2.h"
#include "outputchangeset_v2_p.h"

namespace KWaylandServer
{

OutputChangeSetV2Private::OutputChangeSetV2Private(OutputDeviceV2Interface *outputdevice, OutputChangeSetV2 *parent)
    : q(parent)
    , outputDevice(outputdevice)
    , enabled(outputDevice->enabled())
    , size(outputDevice->pixelSize())
    , refreshRate(outputDevice->refreshRate())
    , transform(outputDevice->transform())
    , position(outputDevice->globalPosition())
    , scale(outputDevice->scale())
    , overscan(outputDevice->overscan())
{
}

OutputChangeSetV2::OutputChangeSetV2(OutputDeviceV2Interface *outputdevice, QObject *parent)
    : QObject(parent)
    , d(new OutputChangeSetV2Private(outputdevice, this))
{
}

OutputChangeSetV2::~OutputChangeSetV2() = default;

bool OutputChangeSetV2::enabledChanged() const
{
    return d->enabled != d->outputDevice->enabled();
}

bool OutputChangeSetV2::enabled() const
{
    return d->enabled;
}

QSize OutputChangeSetV2::size() const
{
    return d->size;
}

bool OutputChangeSetV2::sizeChanged() const
{
    return d->size != d->outputDevice->pixelSize();
}

int OutputChangeSetV2::refreshRate() const
{
    return d->refreshRate;
}

bool OutputChangeSetV2::refreshRateChanged() const
{
    return d->refreshRate != d->outputDevice->refreshRate();
}

bool OutputChangeSetV2::transformChanged() const
{
    return d->transform != d->outputDevice->transform();
}

OutputDeviceV2Interface::Transform OutputChangeSetV2::transform() const
{
    return d->transform;
}
bool OutputChangeSetV2::positionChanged() const
{
    return d->position != d->outputDevice->globalPosition();
}

QPoint OutputChangeSetV2::position() const
{
    return d->position;
}

bool OutputChangeSetV2::scaleChanged() const
{
    return !qFuzzyCompare(d->scale, d->outputDevice->scale());
}

qreal OutputChangeSetV2::scale() const
{
    return d->scale;
}

bool OutputChangeSetV2::overscanChanged() const
{
    return d->overscan != d->outputDevice->overscan();
}

uint32_t OutputChangeSetV2::overscan() const
{
    return d->overscan;
}

bool OutputChangeSetV2::vrrPolicyChanged() const
{
    return d->vrrPolicy != d->outputDevice->vrrPolicy();
}

OutputDeviceV2Interface::VrrPolicy OutputChangeSetV2::vrrPolicy() const
{
    return d->vrrPolicy;
}

}
