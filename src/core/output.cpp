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

void Output::ref()
{
    m_refCount++;
}

void Output::unref()
{
    Q_ASSERT(m_refCount > 0);
    m_refCount--;
    if (m_refCount == 0) {
        delete this;
    }
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
    return m_state.transform;
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

QRect Output::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF Output::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF Output::mapToGlobal(const QRectF &rect) const
{
    return rect.translated(geometry().topLeft());
}

Output::Capabilities Output::capabilities() const
{
    return m_information.capabilities;
}

qreal Output::scale() const
{
    return m_state.scale;
}

QRect Output::geometry() const
{
    return QRect(m_state.position, pixelSize() / scale());
}

QRectF Output::fractionalGeometry() const
{
    return QRectF(m_state.position, QSizeF(pixelSize()) / scale());
}

QSize Output::physicalSize() const
{
    return m_information.physicalSize;
}

int Output::refreshRate() const
{
    return m_state.currentMode ? m_state.currentMode->refreshRate() : 0;
}

QSize Output::modeSize() const
{
    return m_state.currentMode ? m_state.currentMode->size() : QSize();
}

QSize Output::pixelSize() const
{
    return orientateSize(modeSize());
}

QByteArray Output::edid() const
{
    return m_information.edid;
}

QList<std::shared_ptr<OutputMode>> Output::modes() const
{
    return m_state.modes;
}

std::shared_ptr<OutputMode> Output::currentMode() const
{
    return m_state.currentMode;
}

Output::SubPixel Output::subPixel() const
{
    return m_information.subPixel;
}

void Output::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    Q_EMIT aboutToChange();

    State next = m_state;
    next.enabled = props->enabled;
    next.transform = props->transform;
    next.position = props->pos;
    next.scale = props->scale;
    next.rgbRange = props->rgbRange;

    setState(next);
    setVrrPolicy(props->vrrPolicy);

    Q_EMIT changed();
}

bool Output::isEnabled() const
{
    return m_state.enabled;
}

QString Output::description() const
{
    return manufacturer() + ' ' + model();
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

void Output::setState(const State &state)
{
    const QRect oldGeometry = geometry();
    const State oldState = m_state;

    m_state = state;

    if (oldGeometry != geometry()) {
        Q_EMIT geometryChanged();
    }
    if (oldState.scale != state.scale) {
        Q_EMIT scaleChanged();
    }
    if (oldState.modes != state.modes) {
        Q_EMIT modesChanged();
    }
    if (oldState.currentMode != state.currentMode) {
        Q_EMIT currentModeChanged();
    }
    if (oldState.transform != state.transform) {
        Q_EMIT transformChanged();
    }
    if (oldState.overscan != state.overscan) {
        Q_EMIT overscanChanged();
    }
    if (oldState.dpmsMode != state.dpmsMode) {
        Q_EMIT dpmsModeChanged();
    }
    if (oldState.rgbRange != state.rgbRange) {
        Q_EMIT rgbRangeChanged();
    }
    if (oldState.enabled != state.enabled) {
        Q_EMIT enabledChanged();
    }
}

QSize Output::orientateSize(const QSize &size) const
{
    switch (m_state.transform) {
    case Transform::Rotated90:
    case Transform::Rotated270:
    case Transform::Flipped90:
    case Transform::Flipped270:
        return size.transposed();
    default:
        return size;
    }
}

void Output::setDpmsMode(DpmsMode mode)
{
}

Output::DpmsMode Output::dpmsMode() const
{
    return m_state.dpmsMode;
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

uint32_t Output::overscan() const
{
    return m_state.overscan;
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

bool Output::isNonDesktop() const
{
    return m_information.nonDesktop;
}

Output::RgbRange Output::rgbRange() const
{
    return m_state.rgbRange;
}

bool Output::setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation)
{
    return false;
}

bool Output::setCTM(const QMatrix3x3 &ctm)
{
    return false;
}

ContentType Output::contentType() const
{
    return m_contentType;
}

void Output::setContentType(ContentType contentType)
{
    m_contentType = contentType;
}

Output::Transform Output::panelOrientation() const
{
    return m_information.panelOrientation;
}

bool Output::setCursor(CursorSource *source)
{
    return false;
}

bool Output::moveCursor(const QPoint &position)
{
    return false;
}

} // namespace KWin
