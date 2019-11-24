/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "abstract_wayland_output.h"
#include "wayland_server.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/outputchangeset.h>
#include <KWayland/Server/xdgoutput_interface.h>
// KF5
#include <KLocalizedString>

#include <cmath>

namespace KWin
{

AbstractWaylandOutput::AbstractWaylandOutput(QObject *parent)
    : AbstractOutput(parent)
{
}

AbstractWaylandOutput::~AbstractWaylandOutput()
{
    delete m_xdgOutput.data();
    delete m_waylandOutput.data();
    delete m_waylandOutputDevice.data();
}

QString AbstractWaylandOutput::name() const
{
    return QStringLiteral("%1 %2").arg(m_waylandOutputDevice->manufacturer()).arg(
                m_waylandOutputDevice->model());
}

QByteArray AbstractWaylandOutput::uuid() const
{
    return m_waylandOutputDevice->uuid();
}

QRect AbstractWaylandOutput::geometry() const
{
    return QRect(globalPos(), pixelSize() / scale());
}

QSize AbstractWaylandOutput::physicalSize() const
{
    return orientateSize(m_waylandOutputDevice->physicalSize());
}

int AbstractWaylandOutput::refreshRate() const
{
    return m_waylandOutputDevice->refreshRate();
}

QPoint AbstractWaylandOutput::globalPos() const
{
    return m_waylandOutputDevice->globalPosition();
}

void AbstractWaylandOutput::setGlobalPos(const QPoint &pos)
{
    m_waylandOutputDevice->setGlobalPosition(pos);

    if (isEnabled()) {
        m_waylandOutput->setGlobalPosition(pos);
        m_xdgOutput->setLogicalPosition(pos);
        m_xdgOutput->done();
    }
}

QSize AbstractWaylandOutput::pixelSize() const
{
    return orientateSize(m_waylandOutputDevice->pixelSize());
}

qreal AbstractWaylandOutput::scale() const
{
    return m_waylandOutputDevice->scaleF();
}

void AbstractWaylandOutput::setScale(qreal scale)
{
    m_waylandOutputDevice->setScaleF(scale);

    if (isEnabled()) {
        // this is the scale that clients will ideally use for their buffers
        // this has to be an int which is fine

        // I don't know whether we want to round or ceil
        // or maybe even set this to 3 when we're scaling to 1.5
        // don't treat this like it's chosen deliberately
        m_waylandOutput->setScale(std::ceil(scale));
        m_xdgOutput->setLogicalSize(pixelSize() / scale);
        m_xdgOutput->done();
    }
}

using DeviceInterface = KWayland::Server::OutputDeviceInterface;

KWayland::Server::OutputInterface::Transform toOutputTransform(DeviceInterface::Transform transform)
{
    using Transform = DeviceInterface::Transform;
    using OutputTransform = KWayland::Server::OutputInterface::Transform;

    switch (transform) {
    case Transform::Rotated90:
        return OutputTransform::Rotated90;
    case Transform::Rotated180:
        return OutputTransform::Rotated180;
    case Transform::Rotated270:
        return OutputTransform::Rotated270;
    case Transform::Flipped:
        return OutputTransform::Flipped;
    case Transform::Flipped90:
        return OutputTransform::Flipped90;
    case Transform::Flipped180:
        return OutputTransform::Flipped180;
    case Transform::Flipped270:
        return OutputTransform::Flipped270;
    default:
        return OutputTransform::Normal;
    }
}

void AbstractWaylandOutput::setTransform(DeviceInterface::Transform transform)
{
    m_waylandOutputDevice->setTransform(transform);

    if (isEnabled()) {
        m_waylandOutput->setTransform(toOutputTransform(transform));
        m_xdgOutput->setLogicalSize(pixelSize() / scale());
        m_xdgOutput->done();
    }
}

void AbstractWaylandOutput::applyChanges(const KWayland::Server::OutputChangeSet *changeSet)
{
    qCDebug(KWIN_CORE) << "Apply changes to the Wayland output.";
    bool emitModeChanged = false;

    // Enablement changes are handled by platform.
    if (changeSet->modeChanged()) {
        qCDebug(KWIN_CORE) << "Setting new mode:" << changeSet->mode();
        m_waylandOutputDevice->setCurrentMode(changeSet->mode());
        updateMode(changeSet->mode());
        emitModeChanged = true;
    }
    if (changeSet->transformChanged()) {
        qCDebug(KWIN_CORE) << "Server setting transform: " << (int)(changeSet->transform());
        transform(changeSet->transform());
        setTransform(changeSet->transform());
        emitModeChanged = true;
    }
    if (changeSet->positionChanged()) {
        qCDebug(KWIN_CORE) << "Server setting position: " << changeSet->position();
        setGlobalPos(changeSet->position());
        // may just work already!
    }
    if (changeSet->scaleChanged()) {
        qCDebug(KWIN_CORE) << "Setting scale:" << changeSet->scale();
        setScale(changeSet->scaleF());
        emitModeChanged = true;
    }

    if (emitModeChanged) {
        emit modeChanged();
    }
}

bool AbstractWaylandOutput::isEnabled() const
{
    return m_waylandOutputDevice->enabled() == DeviceInterface::Enablement::Enabled;
}

void AbstractWaylandOutput::setEnabled(bool enable)
{
    if (enable == isEnabled()) {
        return;
    }

    if (enable) {
        waylandOutputDevice()->setEnabled(DeviceInterface::Enablement::Enabled);
        createWaylandOutput();
        updateEnablement(true);
    } else {
        waylandOutputDevice()->setEnabled(DeviceInterface::Enablement::Disabled);
        // xdg-output is destroyed in KWayland on wl_output going away.
        delete m_waylandOutput.data();
        updateEnablement(false);
    }
}

void AbstractWaylandOutput::setWaylandMode(const QSize &size, int refreshRate)
{
    if (!isEnabled()) {
        return;
    }
    m_waylandOutput->setCurrentMode(size, refreshRate);
    m_xdgOutput->setLogicalSize(pixelSize() / scale());
    m_xdgOutput->done();
}

void AbstractWaylandOutput::createXdgOutput()
{
    Q_ASSERT(!m_waylandOutput.isNull());
    Q_ASSERT(m_xdgOutput.isNull());

    m_xdgOutput = waylandServer()->xdgOutputManager()->createXdgOutput(m_waylandOutput, m_waylandOutput);
    m_xdgOutput->setLogicalSize(pixelSize() / scale());
    m_xdgOutput->setLogicalPosition(globalPos());
    m_xdgOutput->done();
}

void AbstractWaylandOutput::createWaylandOutput()
{
    Q_ASSERT(m_waylandOutput.isNull());
    m_waylandOutput = waylandServer()->display()->createOutput();
    createXdgOutput();

    /*
     *  add base wayland output data
     */
    m_waylandOutput->setManufacturer(m_waylandOutputDevice->manufacturer());
    m_waylandOutput->setModel(m_waylandOutputDevice->model());
    m_waylandOutput->setPhysicalSize(m_waylandOutputDevice->physicalSize());

    /*
     *  add modes
     */
    for(const auto &mode: m_waylandOutputDevice->modes()) {
        KWayland::Server::OutputInterface::ModeFlags flags;
        if (mode.flags & DeviceInterface::ModeFlag::Current) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Current;
        }
        if (mode.flags & DeviceInterface::ModeFlag::Preferred) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Preferred;
        }
        m_waylandOutput->addMode(mode.size, flags, mode.refreshRate);
    }
    m_waylandOutput->create();

    /*
     *  set dpms
     */
    m_waylandOutput->setDpmsSupported(m_supportsDpms);
    // set to last known mode
    m_waylandOutput->setDpmsMode(m_dpms);
    connect(m_waylandOutput.data(), &KWayland::Server::OutputInterface::dpmsModeRequested, this,
        [this] (KWayland::Server::OutputInterface::DpmsMode mode) {
            updateDpms(mode);
        }
    );
}

void AbstractWaylandOutput::initInterfaces(const QString &model, const QString &manufacturer,
                                           const QByteArray &uuid, const QSize &physicalSize,
                                           const QVector<DeviceInterface::Mode> &modes)
{
    Q_ASSERT(m_waylandOutputDevice.isNull());
    m_waylandOutputDevice = waylandServer()->display()->createOutputDevice();
    m_waylandOutputDevice->setUuid(uuid);

    if (!manufacturer.isEmpty()) {
        m_waylandOutputDevice->setManufacturer(manufacturer);
    } else {
        m_waylandOutputDevice->setManufacturer(i18n("unknown"));
    }

    m_waylandOutputDevice->setModel(model);
    m_waylandOutputDevice->setPhysicalSize(physicalSize);

    int i = 0;
    for (auto mode : modes) {
        qCDebug(KWIN_CORE).nospace() << "Adding mode " << ++i << ": " << mode.size << " [" << mode.refreshRate << "]";
        m_waylandOutputDevice->addMode(mode);
    }

    m_waylandOutputDevice->create();
    createWaylandOutput();
}

QSize AbstractWaylandOutput::orientateSize(const QSize &size) const
{
    using Transform = DeviceInterface::Transform;
    const Transform transform = m_waylandOutputDevice->transform();
    if (transform == Transform::Rotated90 || transform == Transform::Rotated270 ||
            transform == Transform::Flipped90 || transform == Transform::Flipped270) {
        return size.transposed();
    }
    return size;
}

DeviceInterface::Transform toTransform(Qt::ScreenOrientations orientation)
{
    if (orientation | Qt::LandscapeOrientation) {
        if (orientation | Qt::InvertedPortraitOrientation) {
            return DeviceInterface::Transform::Flipped;
        }
        return DeviceInterface::Transform::Normal;
    }

    if (orientation | Qt::PortraitOrientation) {
        if (orientation | Qt::InvertedLandscapeOrientation) {
            if (orientation | Qt::InvertedPortraitOrientation) {
                return DeviceInterface::Transform::Flipped270;
            }
            return DeviceInterface::Transform::Flipped90;
        }
        return DeviceInterface::Transform::Rotated90;
    }

    if (orientation | Qt::InvertedLandscapeOrientation) {
        return DeviceInterface::Transform::Rotated180;
    }

    if (orientation | Qt::InvertedPortraitOrientation) {
        return DeviceInterface::Transform::Rotated270;
    }

    Q_ASSERT(orientation == Qt::PrimaryOrientation);
    return DeviceInterface::Transform::Normal;
}

void AbstractWaylandOutput::setOrientation(Qt::ScreenOrientations orientation)
{
    const auto transform = toTransform(orientation);
    if (transform == m_waylandOutputDevice->transform()) {
        return;
    }
    setTransform(transform);
    emit modeChanged();
}

Qt::ScreenOrientations AbstractWaylandOutput::orientation() const
{
    const DeviceInterface::Transform transform = m_waylandOutputDevice->transform();

    switch (transform) {
    case DeviceInterface::Transform::Rotated90:
        return Qt::PortraitOrientation;
    case DeviceInterface::Transform::Rotated180:
        return Qt::InvertedLandscapeOrientation;
    case DeviceInterface::Transform::Rotated270:
        return Qt::InvertedPortraitOrientation;
    case DeviceInterface::Transform::Flipped:
        return Qt::LandscapeOrientation | Qt::InvertedPortraitOrientation;
    case DeviceInterface::Transform::Flipped90:
        return Qt::PortraitOrientation | Qt::InvertedLandscapeOrientation;
    case DeviceInterface::Transform::Flipped180:
        return Qt::InvertedLandscapeOrientation | Qt::InvertedPortraitOrientation;
    case DeviceInterface::Transform::Flipped270:
        return Qt::PortraitOrientation | Qt::InvertedLandscapeOrientation |
                Qt::InvertedPortraitOrientation;
    default:
        return Qt::LandscapeOrientation;
    }
}

}
