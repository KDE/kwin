/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "output.h"
#include "outputconfiguration.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

namespace KWin
{

QDebug operator<<(QDebug debug, const Output *output)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    if (output) {
        debug << output->metaObject()->className() << '(' << static_cast<const void *>(output);
        debug << ", name=" << output->name();
        debug << ", geometry=" << output->geometry();
        debug << ", scale=" << output->scale();
        if (debug.verbosity() > 2) {
            debug << ", manufacturer=" << output->manufacturer();
            debug << ", model=" << output->model();
            debug << ", serialNumber=" << output->serialNumber();
        }
        debug << ')';
    } else {
        debug << "Output(0x0)";
    }
    return debug;
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

Output::Output(QObject *parent)
    : QObject(parent)
{
}

Output::~Output()
{
}

QString Output::name() const
{
    return m_information.name;
}

QUuid Output::uuid() const
{
    return m_uuid;
}

Output::Transform Output::transform() const
{
    return m_transform;
}

QString Output::eisaId() const
{
    return m_information.eisaId;
}

QString Output::manufacturer() const
{
    return m_information.manufacturer;
}

QString Output::model() const
{
    return m_information.model;
}

QString Output::serialNumber() const
{
    return m_information.serialNumber;
}

bool Output::isInternal() const
{
    return m_information.internal;
}

void Output::inhibitDirectScanout()
{
    m_directScanoutCount++;
}

void Output::uninhibitDirectScanout()
{
    m_directScanoutCount--;
}

bool Output::directScanoutInhibited() const
{
    return m_directScanoutCount;
}

std::chrono::milliseconds Output::dimAnimationTime()
{
    // See kscreen.kcfg
    return std::chrono::milliseconds(KSharedConfig::openConfig()->group("Effect-Kscreen").readEntry("Duration", 250));
}

bool Output::usesSoftwareCursor() const
{
    return true;
}

QRect Output::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

Output::Capabilities Output::capabilities() const
{
    return m_information.capabilities;
}

qreal Output::scale() const
{
    return m_scale;
}

void Output::setScale(qreal scale)
{
    if (m_scale != scale) {
        m_scale = scale;
        Q_EMIT scaleChanged();
        Q_EMIT geometryChanged();
    }
}

QRect Output::geometry() const
{
    return QRect(m_position, pixelSize() / scale());
}

QSize Output::physicalSize() const
{
    return orientateSize(m_information.physicalSize);
}

int Output::refreshRate() const
{
    return m_currentMode->refreshRate();
}

void Output::moveTo(const QPoint &pos)
{
    if (m_position != pos) {
        m_position = pos;
        Q_EMIT geometryChanged();
    }
}

QSize Output::modeSize() const
{
    return m_currentMode->size();
}

QSize Output::pixelSize() const
{
    return orientateSize(m_currentMode->size());
}

QByteArray Output::edid() const
{
    return m_information.edid;
}

QList<QSharedPointer<OutputMode>> Output::modes() const
{
    return m_modes;
}

QSharedPointer<OutputMode> Output::currentMode() const
{
    return m_currentMode;
}

void Output::setModesInternal(const QList<QSharedPointer<OutputMode>> &modes, const QSharedPointer<OutputMode> &currentMode)
{
    const auto oldModes = m_modes;
    const auto oldCurrentMode = m_currentMode;

    m_modes = modes;
    m_currentMode = currentMode;

    if (m_modes != oldModes) {
        Q_EMIT modesChanged();
    }
    if (m_currentMode != oldCurrentMode) {
        Q_EMIT currentModeChanged();
        Q_EMIT geometryChanged();
    }
}

Output::SubPixel Output::subPixel() const
{
    return m_information.subPixel;
}

void Output::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    Q_EMIT aboutToChange();

    setEnabled(props->enabled);
    setTransformInternal(props->transform);
    moveTo(props->pos);
    setScale(props->scale);
    setVrrPolicy(props->vrrPolicy);
    setRgbRangeInternal(props->rgbRange);

    Q_EMIT changed();
}

bool Output::isEnabled() const
{
    return m_isEnabled;
}

void Output::setEnabled(bool enable)
{
    if (m_isEnabled != enable) {
        m_isEnabled = enable;
        updateEnablement(enable);
        Q_EMIT enabledChanged();
    }
}

QString Output::description() const
{
    return manufacturer() + ' ' + model();
}

void Output::setCurrentModeInternal(const QSharedPointer<OutputMode> &currentMode)
{
    if (m_currentMode != currentMode) {
        m_currentMode = currentMode;

        Q_EMIT currentModeChanged();
        Q_EMIT geometryChanged();
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

void Output::setInformation(const Information &information)
{
    m_information = information;
    m_uuid = generateOutputId(eisaId(), model(), serialNumber(), name());
}

QSize Output::orientateSize(const QSize &size) const
{
    if (m_transform == Transform::Rotated90 || m_transform == Transform::Rotated270 || m_transform == Transform::Flipped90 || m_transform == Transform::Flipped270) {
        return size.transposed();
    }
    return size;
}

void Output::setTransformInternal(Transform transform)
{
    if (m_transform != transform) {
        m_transform = transform;
        Q_EMIT transformChanged();
        Q_EMIT currentModeChanged();
        Q_EMIT geometryChanged();
    }
}

void Output::setDpmsModeInternal(DpmsMode dpmsMode)
{
    if (m_dpmsMode != dpmsMode) {
        m_dpmsMode = dpmsMode;
        Q_EMIT dpmsModeChanged();
    }
}

void Output::setDpmsMode(DpmsMode mode)
{
    Q_UNUSED(mode)
}

Output::DpmsMode Output::dpmsMode() const
{
    return m_dpmsMode;
}

QMatrix4x4 Output::logicalToNativeMatrix(const QRect &rect, qreal scale, Transform transform)
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

void Output::setOverscanInternal(uint32_t overscan)
{
    if (m_overscan != overscan) {
        m_overscan = overscan;
        Q_EMIT overscanChanged();
    }
}

uint32_t Output::overscan() const
{
    return m_overscan;
}

void Output::setVrrPolicy(RenderLoop::VrrPolicy policy)
{
    if (renderLoop()->vrrPolicy() != policy && (capabilities() & Capability::Vrr)) {
        renderLoop()->setVrrPolicy(policy);
        Q_EMIT vrrPolicyChanged();
    }
}

RenderLoop::VrrPolicy Output::vrrPolicy() const
{
    return renderLoop()->vrrPolicy();
}

bool Output::isPlaceholder() const
{
    return m_information.placeholder;
}

Output::RgbRange Output::rgbRange() const
{
    return m_rgbRange;
}

void Output::setRgbRangeInternal(RgbRange range)
{
    if (m_rgbRange != range) {
        m_rgbRange = range;
        Q_EMIT rgbRangeChanged();
    }
}

void Output::setColorTransformation(const QSharedPointer<ColorTransformation> &transformation)
{
    Q_UNUSED(transformation);
}

} // namespace KWin
