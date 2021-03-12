/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "outputchangeset.h"
#include "outputchangeset_p.h"

namespace KWaylandServer
{

OutputChangeSetPrivate::OutputChangeSetPrivate(OutputDeviceInterface *outputdevice, OutputChangeSet *parent)
    : q(parent)
    , outputDevice(outputdevice)
    , enabled(outputDevice->enabled())
    , modeId(outputDevice->currentModeId())
    , transform(outputDevice->transform())
    , position(outputDevice->globalPosition())
    , scale(outputDevice->scaleF())
    , colorCurves(outputDevice->colorCurves())
{
}

OutputChangeSet::OutputChangeSet(OutputDeviceInterface *outputdevice, QObject *parent)
    : QObject(parent)
    , d(new OutputChangeSetPrivate(outputdevice, this))
{
}

OutputChangeSet::~OutputChangeSet() = default;

bool OutputChangeSet::enabledChanged() const
{
    return d->enabled != d->outputDevice->enabled();
}

OutputDeviceInterface::Enablement OutputChangeSet::enabled() const
{
    return d->enabled;
}

bool OutputChangeSet::modeChanged() const
{
    return d->modeId != d->outputDevice->currentModeId();
}

int OutputChangeSet::mode() const
{
    return d->modeId;
}

bool OutputChangeSet::transformChanged() const
{
    return d->transform != d->outputDevice->transform();
}

OutputDeviceInterface::Transform OutputChangeSet::transform() const
{
    return d->transform;
}
bool OutputChangeSet::positionChanged() const
{
    return d->position != d->outputDevice->globalPosition();
}

QPoint OutputChangeSet::position() const
{
    return d->position;
}

bool OutputChangeSet::scaleChanged() const
{
    return !qFuzzyCompare(d->scale, d->outputDevice->scaleF());
}

qreal OutputChangeSet::scaleF() const
{
    return d->scale;
}

bool OutputChangeSet::colorCurvesChanged() const
{
    return d->colorCurves != d->outputDevice->colorCurves();
}

OutputDeviceInterface::ColorCurves OutputChangeSet::colorCurves() const
{
    return d->colorCurves;
}

}
