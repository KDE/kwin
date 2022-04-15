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

GammaRamp::GammaRamp(uint32_t size)
    : m_table(3 * size)
    , m_size(size)
{
}

uint32_t GammaRamp::size() const
{
    return m_size;
}

uint16_t *GammaRamp::red()
{
    return m_table.data();
}

const uint16_t *GammaRamp::red() const
{
    return m_table.data();
}

uint16_t *GammaRamp::green()
{
    return m_table.data() + m_size;
}

const uint16_t *GammaRamp::green() const
{
    return m_table.data() + m_size;
}

uint16_t *GammaRamp::blue()
{
    return m_table.data() + 2 * m_size;
}

const uint16_t *GammaRamp::blue() const
{
    return m_table.data() + 2 * m_size;
}

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

Output::Output(QObject *parent)
    : QObject(parent)
{
}

Output::~Output()
{
}

QString Output::name() const
{
    return m_name;
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
    return m_eisaId;
}

QString Output::manufacturer() const
{
    return m_manufacturer;
}

QString Output::model() const
{
    return m_model;
}

QString Output::serialNumber() const
{
    return m_serialNumber;
}

bool Output::isInternal() const
{
    return m_internal;
}

int Output::gammaRampSize() const
{
    return 0;
}

bool Output::setGammaRamp(const GammaRamp &gamma)
{
    Q_UNUSED(gamma);
    return false;
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
    return m_capabilities;
}

void Output::setCapabilityInternal(Capability capability, bool on)
{
    if (static_cast<bool>(m_capabilities & capability) != on) {
        m_capabilities.setFlag(capability, on);
        Q_EMIT capabilitiesChanged();
    }
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
    return orientateSize(m_physicalSize);
}

int Output::refreshRate() const
{
    return m_refreshRate;
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
    return m_modeSize;
}

QSize Output::pixelSize() const
{
    return orientateSize(m_modeSize);
}

QByteArray Output::edid() const
{
    return m_edid;
}

bool Output::Mode::operator==(const Mode &other) const
{
    return id == other.id && other.flags == flags && size == other.size && refreshRate == other.refreshRate;
}

QVector<Output::Mode> Output::modes() const
{
    return m_modes;
}

void Output::setModes(const QVector<Mode> &modes)
{
    if (m_modes != modes) {
        m_modes = modes;
        Q_EMIT modesChanged();
    }
}

Output::SubPixel Output::subPixel() const
{
    return m_subPixel;
}

void Output::setSubPixelInternal(SubPixel subPixel)
{
    m_subPixel = subPixel;
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
    return m_manufacturer + ' ' + m_model;
}

void Output::setCurrentModeInternal(const QSize &size, int refreshRate)
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

void Output::initialize(const QString &model, const QString &manufacturer,
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
    if (renderLoop()->vrrPolicy() != policy && (m_capabilities & Capability::Vrr)) {
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
    return m_isPlaceholder;
}

void Output::setPlaceholder(bool isPlaceholder)
{
    m_isPlaceholder = isPlaceholder;
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

void Output::setPhysicalSizeInternal(const QSize &size)
{
    m_physicalSize = size;
}

} // namespace KWin
