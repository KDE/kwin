/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_wayland_output.h"

#include "screens.h"
#include "wayland_server.h"

// KWayland
#include <KWaylandServer/display.h>
#include <KWaylandServer/outputchangeset.h>
#include <KWaylandServer/xdgoutput_v1_interface.h>
// KF5
#include <KLocalizedString>

#include <QMatrix4x4>
#include <cmath>

namespace KWin
{

AbstractWaylandOutput::AbstractWaylandOutput(QObject *parent)
    : AbstractOutput(parent)
{
    m_waylandOutput = waylandServer()->display()->createOutput(this);
    m_waylandOutputDevice = waylandServer()->display()->createOutputDevice(this);
    m_xdgOutputV1 = waylandServer()->xdgOutputManagerV1()->createXdgOutput(m_waylandOutput, this);

    connect(m_waylandOutput, &KWaylandServer::OutputInterface::dpmsModeRequested, this,
            [this] (KWaylandServer::OutputInterface::DpmsMode mode) {
        updateDpms(mode);
    });

    connect(m_waylandOutput, &KWaylandServer::OutputInterface::globalPositionChanged, this, &AbstractWaylandOutput::geometryChanged);
    connect(m_waylandOutput, &KWaylandServer::OutputInterface::pixelSizeChanged, this, &AbstractWaylandOutput::geometryChanged);
    connect(m_waylandOutput, &KWaylandServer::OutputInterface::scaleChanged, this, &AbstractWaylandOutput::geometryChanged);
}

AbstractWaylandOutput::~AbstractWaylandOutput()
{
}

QString AbstractWaylandOutput::name() const
{
    return m_name;
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

    m_waylandOutput->setGlobalPosition(pos);
    m_xdgOutputV1->setLogicalPosition(pos);
    m_xdgOutputV1->done();
}

QSize AbstractWaylandOutput::modeSize() const
{
    return m_waylandOutputDevice->pixelSize();
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

    // this is the scale that clients will ideally use for their buffers
    // this has to be an int which is fine

    // I don't know whether we want to round or ceil
    // or maybe even set this to 3 when we're scaling to 1.5
    // don't treat this like it's chosen deliberately
    m_waylandOutput->setScale(std::ceil(scale));
    m_xdgOutputV1->setLogicalSize(pixelSize() / scale);
    m_xdgOutputV1->done();
}

using DeviceInterface = KWaylandServer::OutputDeviceInterface;

KWaylandServer::OutputInterface::Transform toOutputTransform(DeviceInterface::Transform transform)
{
    using Transform = DeviceInterface::Transform;
    using OutputTransform = KWaylandServer::OutputInterface::Transform;

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

    m_waylandOutput->setTransform(toOutputTransform(transform));
    m_xdgOutputV1->setLogicalSize(pixelSize() / scale());
    m_xdgOutputV1->done();
}

inline
AbstractWaylandOutput::Transform toTransform(DeviceInterface::Transform deviceTransform)
{
    return static_cast<AbstractWaylandOutput::Transform>(deviceTransform);
}

inline
DeviceInterface::Transform toDeviceTransform(AbstractWaylandOutput::Transform transform)
{
    return static_cast<DeviceInterface::Transform>(transform);
}

void AbstractWaylandOutput::applyChanges(const KWaylandServer::OutputChangeSet *changeSet)
{
    qCDebug(KWIN_CORE) << "Apply changes to the Wayland output.";
    bool emitModeChanged = false;
    bool overallSizeCheckNeeded = false;

    // Enablement changes are handled by platform.
    if (changeSet->modeChanged()) {
        qCDebug(KWIN_CORE) << "Setting new mode:" << changeSet->mode();
        m_waylandOutputDevice->setCurrentMode(changeSet->mode());
        updateMode(changeSet->mode());
        emitModeChanged = true;
    }
    if (changeSet->transformChanged()) {
        qCDebug(KWIN_CORE) << "Server setting transform: " << (int)(changeSet->transform());
        setTransform(changeSet->transform());
        updateTransform(toTransform(changeSet->transform()));
        emitModeChanged = true;
    }
    if (changeSet->positionChanged()) {
        qCDebug(KWIN_CORE) << "Server setting position: " << changeSet->position();
        setGlobalPos(changeSet->position());
        // may just work already!
        overallSizeCheckNeeded = true;
    }
    if (changeSet->scaleChanged()) {
        qCDebug(KWIN_CORE) << "Setting scale:" << changeSet->scaleF();
        setScale(changeSet->scaleF());
        emitModeChanged = true;
    }

    overallSizeCheckNeeded |= emitModeChanged;
    if (overallSizeCheckNeeded) {
        emit screens()->changed();
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
        m_waylandOutputDevice->setEnabled(DeviceInterface::Enablement::Enabled);
        m_waylandOutput->create();
        updateEnablement(true);
    } else {
        m_waylandOutputDevice->setEnabled(DeviceInterface::Enablement::Disabled);
        m_waylandOutput->destroy();
        // xdg-output is destroyed in KWayland on wl_output going away.
        updateEnablement(false);
    }
}

QString AbstractWaylandOutput::description() const
{
    return QStringLiteral("%1 %2").arg(m_waylandOutputDevice->manufacturer()).arg(
                m_waylandOutputDevice->model());
}

void AbstractWaylandOutput::setWaylandMode(const QSize &size, int refreshRate)
{
    m_waylandOutput->setCurrentMode(size, refreshRate);
    m_xdgOutputV1->setLogicalSize(pixelSize() / scale());
    m_xdgOutputV1->done();
}

void AbstractWaylandOutput::initInterfaces(const QString &model, const QString &manufacturer,
                                           const QByteArray &uuid, const QSize &physicalSize,
                                           const QVector<DeviceInterface::Mode> &modes)
{
    m_waylandOutputDevice->setUuid(uuid);

    if (!manufacturer.isEmpty()) {
        m_waylandOutputDevice->setManufacturer(manufacturer);
    } else {
        m_waylandOutputDevice->setManufacturer(i18n("unknown"));
    }

    m_waylandOutputDevice->setModel(model);
    m_waylandOutputDevice->setPhysicalSize(physicalSize);

    m_waylandOutput->setManufacturer(m_waylandOutputDevice->manufacturer());
    m_waylandOutput->setModel(m_waylandOutputDevice->model());
    m_waylandOutput->setPhysicalSize(m_waylandOutputDevice->physicalSize());

    int i = 0;
    for (auto mode : modes) {
        qCDebug(KWIN_CORE).nospace() << "Adding mode " << ++i << ": " << mode.size << " [" << mode.refreshRate << "]";
        m_waylandOutputDevice->addMode(mode);

        KWaylandServer::OutputInterface::ModeFlags flags;
        if (mode.flags & DeviceInterface::ModeFlag::Current) {
            flags |= KWaylandServer::OutputInterface::ModeFlag::Current;
        }
        if (mode.flags & DeviceInterface::ModeFlag::Preferred) {
            flags |= KWaylandServer::OutputInterface::ModeFlag::Preferred;
        }
        m_waylandOutput->addMode(mode.size, flags, mode.refreshRate);
    }

    m_waylandOutputDevice->create();

    // start off enabled

    m_waylandOutput->create();
    m_xdgOutputV1->setName(name());
    m_xdgOutputV1->setDescription(description());
    m_xdgOutputV1->setLogicalSize(pixelSize() / scale());
    m_xdgOutputV1->done();
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

void AbstractWaylandOutput::setTransform(Transform transform)
{
    const auto deviceTransform = toDeviceTransform(transform);
    if (deviceTransform == m_waylandOutputDevice->transform()) {
        return;
    }
    setTransform(deviceTransform);
    emit modeChanged();
}

AbstractWaylandOutput::Transform AbstractWaylandOutput::transform() const
{
    return static_cast<Transform>(m_waylandOutputDevice->transform());
}

QMatrix4x4 AbstractWaylandOutput::logicalToNativeMatrix(const QRect &rect, qreal scale, Transform transform)
{
    QMatrix4x4 matrix;
    matrix.scale(scale);

    switch (transform) {
    case Transform::Normal:
    case Transform::Flipped:
        break;
    case Transform::Rotated90:
    case Transform::Flipped90:
        matrix.translate(0, rect.width());
        matrix.rotate(-90, 0, 0, 1);
        break;
    case Transform::Rotated180:
    case Transform::Flipped180:
        matrix.translate(rect.width(), rect.height());
        matrix.rotate(-180, 0, 0, 1);
        break;
    case Transform::Rotated270:
    case Transform::Flipped270:
        matrix.translate(rect.height(), 0);
        matrix.rotate(-270, 0, 0, 1);
        break;
    }

    switch (transform) {
    case Transform::Flipped:
    case Transform::Flipped90:
    case Transform::Flipped180:
    case Transform::Flipped270:
        matrix.translate(rect.width(), 0);
        matrix.scale(-1, 1);
        break;
    default:
        break;
    }

    matrix.translate(-rect.x(), -rect.y());

    return matrix;
}

}
