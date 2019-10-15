/****************************************************************************
 * Copyright 2015  Sebastian Kügler <sebas@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include "outputchangeset.h"
#include "outputchangeset_p.h"

namespace KWayland
{
namespace Server
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
}
