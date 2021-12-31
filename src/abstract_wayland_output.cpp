/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_wayland_output.h"
#include "screens.h"
#include "waylandoutputconfig.h"

// KWayland
#include <KWaylandServer/outputchangeset_v2.h>
// KF5
#include <KLocalizedString>

#include <QMatrix4x4>

namespace KWin
{

AbstractWaylandOutput::AbstractWaylandOutput(QObject *parent)
    : AbstractOutput(parent)
{
}

AbstractWaylandOutput::Capabilities AbstractWaylandOutput::capabilities() const
{
    return m_capabilities;
}

void AbstractWaylandOutput::setCapabilityInternal(Capability capability, bool on)
{
    if (static_cast<bool>(m_capabilities & capability) != on) {
        m_capabilities.setFlag(capability, on);
        Q_EMIT capabilitiesChanged();
    }
}

QString AbstractWaylandOutput::name() const
{
    return m_name;
}

QUuid AbstractWaylandOutput::uuid() const
{
    return m_uuid;
}

QRect AbstractWaylandOutput::geometry() const
{
    return QRect(m_position, pixelSize() / scale());
}

QSize AbstractWaylandOutput::physicalSize() const
{
    return orientateSize(m_physicalSize);
}

int AbstractWaylandOutput::refreshRate() const
{
    return m_refreshRate;
}

void AbstractWaylandOutput::moveTo(const QPoint &pos)
{
    if (m_position != pos) {
        m_position = pos;
        Q_EMIT geometryChanged();
    }
}

QString AbstractWaylandOutput::eisaId() const
{
    return m_eisaId;
}

QString AbstractWaylandOutput::manufacturer() const
{
    return m_manufacturer;
}

QString AbstractWaylandOutput::model() const
{
    return m_model;
}

QString AbstractWaylandOutput::serialNumber() const
{
    return m_serialNumber;
}

QSize AbstractWaylandOutput::modeSize() const
{
    return m_modeSize;
}

QSize AbstractWaylandOutput::pixelSize() const
{
    return orientateSize(m_modeSize);
}

QByteArray AbstractWaylandOutput::edid() const
{
    return m_edid;
}

bool AbstractWaylandOutput::Mode::operator==(const Mode &other) const {
    return id == other.id && other.flags == flags && size == other.size && refreshRate == other.refreshRate;
}

QVector<AbstractWaylandOutput::Mode> AbstractWaylandOutput::modes() const
{
    return m_modes;
}

void AbstractWaylandOutput::setModes(const QVector<Mode> &modes)
{
    if (m_modes != modes) {
        m_modes = modes;
        Q_EMIT modesChanged();
    }
}

qreal AbstractWaylandOutput::scale() const
{
    return m_scale;
}

void AbstractWaylandOutput::setScale(qreal scale)
{
    if (m_scale != scale) {
        m_scale = scale;
        Q_EMIT scaleChanged();
        Q_EMIT geometryChanged();
    }
}

AbstractWaylandOutput::SubPixel AbstractWaylandOutput::subPixel() const
{
    return m_subPixel;
}

void AbstractWaylandOutput::setSubPixelInternal(SubPixel subPixel)
{
    m_subPixel = subPixel;
}

void AbstractWaylandOutput::applyChanges(const WaylandOutputConfig &config)
{
    auto props = config.constChangeSet(this);
    Q_EMIT aboutToChange();

    setEnabled(props->enabled);
    updateTransform(props->transform);
    moveTo(props->pos);
    setScale(props->scale);
    setVrrPolicy(props->vrrPolicy);
    setRgbRangeInternal(props->rgbRange);

    Q_EMIT changed();
}

bool AbstractWaylandOutput::isEnabled() const
{
    return m_isEnabled;
}

void AbstractWaylandOutput::setEnabled(bool enable)
{
    if (m_isEnabled != enable) {
        m_isEnabled = enable;
        updateEnablement(enable);
        Q_EMIT enabledChanged();
    }
}

QString AbstractWaylandOutput::description() const
{
    return m_manufacturer + ' ' + m_model;
}

void AbstractWaylandOutput::setCurrentModeInternal(const QSize &size, int refreshRate)
{
    const bool sizeChanged = m_modeSize != size;
    if (sizeChanged || m_refreshRate != refreshRate) {
        m_modeSize = size;
        m_refreshRate = refreshRate;

        Q_EMIT currentModeChanged();
        if (sizeChanged) {
            Q_EMIT geometryChanged();
        }
    }
}

static QUuid generateOutputId(const QString &eisaId, const QString &model,
                              const QString &serialNumber, const QString &name)
{
    static const QUuid urlNs = QUuid("6ba7b811-9dad-11d1-80b4-00c04fd430c8"); // NameSpace_URL
    static const QUuid kwinNs = QUuid::createUuidV5(urlNs, QStringLiteral("https://kwin.kde.org/o/"));

    const QString payload = QStringList{name, eisaId, model, serialNumber}.join(':');
    return QUuid::createUuidV5(kwinNs, payload);
}

void AbstractWaylandOutput::initialize(const QString &model, const QString &manufacturer,
                                       const QString &eisaId, const QString &serialNumber,
                                       const QSize &physicalSize,
                                       const QVector<Mode> &modes, const QByteArray &edid)
{
    m_serialNumber = serialNumber;
    m_eisaId = eisaId;
    m_manufacturer = manufacturer.isEmpty() ? i18n("unknown") : manufacturer;
    m_model = model;
    m_physicalSize = physicalSize;
    m_edid = edid;
    m_modes = modes;
    m_uuid = generateOutputId(m_eisaId, m_model, m_serialNumber, m_name);

    for (const Mode &mode : modes) {
        if (mode.flags & ModeFlag::Current) {
            m_modeSize = mode.size;
            m_refreshRate = mode.refreshRate;
            break;
        }
    }
}

QSize AbstractWaylandOutput::orientateSize(const QSize &size) const
{
    if (m_transform == Transform::Rotated90 || m_transform == Transform::Rotated270 ||
            m_transform == Transform::Flipped90 || m_transform == Transform::Flipped270) {
        return size.transposed();
    }
    return size;
}

void AbstractWaylandOutput::setTransformInternal(Transform transform)
{
    if (m_transform != transform) {
        m_transform = transform;
        Q_EMIT transformChanged();
        Q_EMIT currentModeChanged();
        Q_EMIT geometryChanged();
    }
}

AbstractWaylandOutput::Transform AbstractWaylandOutput::transform() const
{
    return m_transform;
}

void AbstractWaylandOutput::setDpmsModeInternal(DpmsMode dpmsMode)
{
    if (m_dpmsMode != dpmsMode) {
        m_dpmsMode = dpmsMode;
        Q_EMIT dpmsModeChanged();
    }
}

void AbstractWaylandOutput::setDpmsMode(DpmsMode mode)
{
    Q_UNUSED(mode)
}

AbstractWaylandOutput::DpmsMode AbstractWaylandOutput::dpmsMode() const
{
    return m_dpmsMode;
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

void AbstractWaylandOutput::recordingStarted()
{
    m_recorders++;
}

void AbstractWaylandOutput::recordingStopped()
{
    m_recorders--;
}

bool AbstractWaylandOutput::isBeingRecorded()
{
    return m_recorders;
}

void AbstractWaylandOutput::setOverscanInternal(uint32_t overscan)
{
    if (m_overscan != overscan) {
        m_overscan = overscan;
        Q_EMIT overscanChanged();
    }
}

uint32_t AbstractWaylandOutput::overscan() const
{
    return m_overscan;
}

void AbstractWaylandOutput::setVrrPolicy(RenderLoop::VrrPolicy policy)
{
    if (renderLoop()->vrrPolicy() != policy && (m_capabilities & Capability::Vrr)) {
        renderLoop()->setVrrPolicy(policy);
        Q_EMIT vrrPolicyChanged();
    }
}

RenderLoop::VrrPolicy AbstractWaylandOutput::vrrPolicy() const
{
    return renderLoop()->vrrPolicy();
}

bool AbstractWaylandOutput::isPlaceholder() const
{
    return m_isPlaceholder;
}

void AbstractWaylandOutput::setPlaceholder(bool isPlaceholder)
{
    m_isPlaceholder = isPlaceholder;
}

AbstractWaylandOutput::RgbRange AbstractWaylandOutput::rgbRange() const
{
    return m_rgbRange;
}

void AbstractWaylandOutput::setRgbRangeInternal(RgbRange range)
{
    if (m_rgbRange != range) {
        m_rgbRange = range;
        Q_EMIT rgbRangeChanged();
    }
}

}
