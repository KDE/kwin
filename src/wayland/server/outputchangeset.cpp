/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "outputchangeset.h"
#include "outputchangeset_p.h"

namespace KWaylandServer
{

OutputChangeSet::Private::Private(OutputDeviceInterface *outputdevice, OutputChangeSet *parent)
    : q(parent)
    , o(outputdevice)
    , enabled(o->enabled())
    , modeId(o->currentModeId())
    , transform(o->transform())
    , position(o->globalPosition())
    , scale(o->scaleF())
    , colorCurves(o->colorCurves())
{
}

OutputChangeSet::Private::~Private() = default;

OutputChangeSet::OutputChangeSet(OutputDeviceInterface *outputdevice, QObject *parent)
    : QObject(parent)
    , d(new Private(outputdevice, this))
{
}

OutputChangeSet::~OutputChangeSet() = default;

OutputChangeSet::Private *OutputChangeSet::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

bool OutputChangeSet::enabledChanged() const
{
    Q_D();
    return d->enabled != d->o->enabled();
}

OutputDeviceInterface::Enablement OutputChangeSet::enabled() const
{
    Q_D();
    return d->enabled;
}

bool OutputChangeSet::modeChanged() const
{
    Q_D();
    return d->modeId != d->o->currentModeId();
}

int OutputChangeSet::mode() const
{
    Q_D();
    return d->modeId;
}

bool OutputChangeSet::transformChanged() const
{
    Q_D();
    return d->transform != d->o->transform();
}

OutputDeviceInterface::Transform OutputChangeSet::transform() const
{
    Q_D();
    return d->transform;
}
bool OutputChangeSet::positionChanged() const
{
    Q_D();
    return d->position != d->o->globalPosition();
}

QPoint OutputChangeSet::position() const
{
    Q_D();
    return d->position;
}

bool OutputChangeSet::scaleChanged() const
{
    Q_D();
    return !qFuzzyCompare(d->scale, d->o->scaleF());
}

int OutputChangeSet::scale() const
{
    Q_D();
    return qRound(d->scale);
}

qreal OutputChangeSet::scaleF() const
{
    Q_D();
    return d->scale;
}

bool OutputChangeSet::colorCurvesChanged() const
{
    Q_D();
    return d->colorCurves != d->o->colorCurves();
}

OutputDeviceInterface::ColorCurves OutputChangeSet::colorCurves() const
{
    Q_D();
    return d->colorCurves;
}

}
