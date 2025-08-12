/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "output.h"
#include "brightnessdevice.h"
#include "iccprofile.h"
#include "outputconfiguration.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QJSEngine>

namespace KWin
{

QDebug operator<<(QDebug debug, const LogicalOutput *output)
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
        debug << "LogicalOutput(0x0)";
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

void OutputMode::setRemoved()
{
    m_flags |= OutputMode::Flag::Removed;
}

OutputTransform::Kind OutputTransform::kind() const
{
    return m_kind;
}

OutputTransform OutputTransform::inverted() const
{
    switch (m_kind) {
    case Kind::Normal:
        return Kind::Normal;
    case Kind::Rotate90:
        return Kind::Rotate270;
    case Kind::Rotate180:
        return Kind::Rotate180;
    case Kind::Rotate270:
        return Kind::Rotate90;
    case Kind::FlipX:
    case Kind::FlipX90:
    case Kind::FlipX180:
    case Kind::FlipX270:
        return m_kind; // inverse transform of a flip transform is itself
    }

    Q_UNREACHABLE();
}

QRectF OutputTransform::map(const QRectF &rect, const QSizeF &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return rect;
    case Kind::Rotate90:
        return QRectF(rect.y(),
                      bounds.width() - (rect.x() + rect.width()),
                      rect.height(),
                      rect.width());
    case Kind::Rotate180:
        return QRectF(bounds.width() - (rect.x() + rect.width()),
                      bounds.height() - (rect.y() + rect.height()),
                      rect.width(),
                      rect.height());
    case Kind::Rotate270:
        return QRectF(bounds.height() - (rect.y() + rect.height()),
                      rect.x(),
                      rect.height(),
                      rect.width());
    case Kind::FlipX:
        return QRectF(bounds.width() - (rect.x() + rect.width()),
                      rect.y(),
                      rect.width(),
                      rect.height());
    case Kind::FlipX90:
        return QRectF(rect.y(),
                      rect.x(),
                      rect.height(),
                      rect.width());
    case Kind::FlipX180:
        return QRectF(rect.x(),
                      bounds.height() - (rect.y() + rect.height()),
                      rect.width(),
                      rect.height());
    case Kind::FlipX270:
        return QRectF(bounds.height() - (rect.y() + rect.height()),
                      bounds.width() - (rect.x() + rect.width()),
                      rect.height(),
                      rect.width());
    default:
        Q_UNREACHABLE();
    }
}

QRect OutputTransform::map(const QRect &rect, const QSize &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return rect;
    case Kind::Rotate90:
        return QRect(rect.y(),
                     bounds.width() - (rect.x() + rect.width()),
                     rect.height(),
                     rect.width());
    case Kind::Rotate180:
        return QRect(bounds.width() - (rect.x() + rect.width()),
                     bounds.height() - (rect.y() + rect.height()),
                     rect.width(),
                     rect.height());
    case Kind::Rotate270:
        return QRect(bounds.height() - (rect.y() + rect.height()),
                     rect.x(),
                     rect.height(),
                     rect.width());
    case Kind::FlipX:
        return QRect(bounds.width() - (rect.x() + rect.width()),
                     rect.y(),
                     rect.width(),
                     rect.height());
    case Kind::FlipX90:
        return QRect(rect.y(),
                     rect.x(),
                     rect.height(),
                     rect.width());
    case Kind::FlipX180:
        return QRect(rect.x(),
                     bounds.height() - (rect.y() + rect.height()),
                     rect.width(),
                     rect.height());
    case Kind::FlipX270:
        return QRect(bounds.height() - (rect.y() + rect.height()),
                     bounds.width() - (rect.x() + rect.width()),
                     rect.height(),
                     rect.width());
    default:
        Q_UNREACHABLE();
    }
}

QPointF OutputTransform::map(const QPointF &point, const QSizeF &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return point;
    case Kind::Rotate90:
        return QPointF(point.y(),
                       bounds.width() - point.x());
    case Kind::Rotate180:
        return QPointF(bounds.width() - point.x(),
                       bounds.height() - point.y());
    case Kind::Rotate270:
        return QPointF(bounds.height() - point.y(),
                       point.x());
    case Kind::FlipX:
        return QPointF(bounds.width() - point.x(),
                       point.y());
    case Kind::FlipX90:
        return QPointF(point.y(),
                       point.x());
    case Kind::FlipX180:
        return QPointF(point.x(),
                       bounds.height() - point.y());
    case Kind::FlipX270:
        return QPointF(bounds.height() - point.y(),
                       bounds.width() - point.x());
    default:
        Q_UNREACHABLE();
    }
}

QPoint OutputTransform::map(const QPoint &point, const QSize &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return point;
    case Kind::Rotate90:
        return QPoint(point.y(),
                      bounds.width() - point.x());
    case Kind::Rotate180:
        return QPoint(bounds.width() - point.x(),
                      bounds.height() - point.y());
    case Kind::Rotate270:
        return QPoint(bounds.height() - point.y(),
                      point.x());
    case Kind::FlipX:
        return QPoint(bounds.width() - point.x(),
                      point.y());
    case Kind::FlipX90:
        return QPoint(point.y(),
                      point.x());
    case Kind::FlipX180:
        return QPoint(point.x(),
                      bounds.height() - point.y());
    case Kind::FlipX270:
        return QPoint(bounds.height() - point.y(),
                      bounds.width() - point.x());
    default:
        Q_UNREACHABLE();
    }
}

QSizeF OutputTransform::map(const QSizeF &size) const
{
    switch (m_kind) {
    case Kind::Normal:
    case Kind::Rotate180:
    case Kind::FlipX:
    case Kind::FlipX180:
        return size;
    default:
        return size.transposed();
    }
}

QSize OutputTransform::map(const QSize &size) const
{
    switch (m_kind) {
    case Kind::Normal:
    case Kind::Rotate180:
    case Kind::FlipX:
    case Kind::FlipX180:
        return size;
    default:
        return size.transposed();
    }
}

OutputTransform OutputTransform::combine(OutputTransform other) const
{
    // Combining a rotate-N or flip-N (mirror-x | rotate-N) transform with a rotate-M
    // transform involves only adding rotation angles:
    //     rotate-N | rotate-M => rotate-(N + M)
    //     flip-N | rotate-M => mirror-x | rotate-N | rotate-M
    //         => mirror-x | rotate-(N + M)
    //         => flip-(N + M)
    //
    // rotate-N | mirror-x is the same as mirror-x | rotate-(360 - N). This can be used
    // to derive the resulting transform if the other transform flips the x axis
    //     rotate-N | flip-M => rotate-N | mirror-x | rotate-M
    //        => mirror-x | rotate-(360 - N + M)
    //        => flip-(M - N)
    //     flip-N | flip-M => mirror-x | rotate-N | mirror-x | rotate-M
    //         => mirror-x | mirror-x | rotate-(360 - N + M)
    //         => rotate-(360 - N + M)
    //         => rotate-(M - N)
    //
    // The remaining code here relies on the bit pattern of transform enums, i.e. the
    // lower two bits specify the rotation, the third bit indicates mirroring along the x axis.

    const int flip = (m_kind ^ other.m_kind) & 0x4;
    int rotate;
    if (other.m_kind & 0x4) {
        rotate = (other.m_kind - m_kind) & 0x3;
    } else {
        rotate = (m_kind + other.m_kind) & 0x3;
    }
    return OutputTransform(Kind(flip | rotate));
}

QMatrix4x4 OutputTransform::toMatrix() const
{
    QMatrix4x4 matrix;
    switch (m_kind) {
    case Kind::Normal:
        break;
    case Kind::Rotate90:
        matrix.rotate(-90, 0, 0, 1);
        break;
    case Kind::Rotate180:
        matrix.rotate(-180, 0, 0, 1);
        break;
    case Kind::Rotate270:
        matrix.rotate(-270, 0, 0, 1);
        break;
    case Kind::FlipX:
        matrix.scale(-1, 1);
        break;
    case Kind::FlipX90:
        matrix.rotate(-90, 0, 0, 1);
        matrix.scale(-1, 1);
        break;
    case Kind::FlipX180:
        matrix.rotate(-180, 0, 0, 1);
        matrix.scale(-1, 1);
        break;
    case Kind::FlipX270:
        matrix.rotate(-270, 0, 0, 1);
        matrix.scale(-1, 1);
        break;
    default:
        Q_UNREACHABLE();
    }
    return matrix;
}

LogicalOutput::LogicalOutput(QObject *parent)
    : QObject(parent)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

LogicalOutput::~LogicalOutput()
{
}

void LogicalOutput::ref()
{
    m_refCount++;
}

void LogicalOutput::unref()
{
    Q_ASSERT(m_refCount > 0);
    m_refCount--;
    if (m_refCount == 0) {
        delete this;
    }
}

QString LogicalOutput::name() const
{
    return m_information.name;
}

QString LogicalOutput::uuid() const
{
    return m_state.uuid;
}

OutputTransform LogicalOutput::transform() const
{
    return m_state.transform;
}

OutputTransform LogicalOutput::manualTransform() const
{
    return m_state.manualTransform;
}

QString LogicalOutput::eisaId() const
{
    return m_information.eisaId;
}

QString LogicalOutput::manufacturer() const
{
    return m_information.manufacturer;
}

QString LogicalOutput::model() const
{
    return m_information.model;
}

QString LogicalOutput::serialNumber() const
{
    return m_information.serialNumber;
}

bool LogicalOutput::isInternal() const
{
    return m_information.internal;
}

std::chrono::milliseconds LogicalOutput::dimAnimationTime()
{
    // See kscreen.kcfg
    return std::chrono::milliseconds(KSharedConfig::openConfig()->group(QStringLiteral("Effect-Kscreen")).readEntry("Duration", 250));
}

QRect LogicalOutput::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF LogicalOutput::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-geometry().topLeft());
}

QRectF LogicalOutput::mapToGlobal(const QRectF &rect) const
{
    return rect.translated(geometry().topLeft());
}

QRegion LogicalOutput::mapToGlobal(const QRegion &region) const
{
    return region.translated(geometry().topLeft());
}

QPointF LogicalOutput::mapToGlobal(const QPointF &pos) const
{
    return pos + geometry().topLeft();
}

QPointF LogicalOutput::mapFromGlobal(const QPointF &pos) const
{
    return pos - geometry().topLeft();
}

LogicalOutput::Capabilities LogicalOutput::capabilities() const
{
    return m_information.capabilities;
}

qreal LogicalOutput::scale() const
{
    return m_state.scale;
}

QRect LogicalOutput::geometry() const
{
    return QRect(m_state.position, pixelSize() / scale());
}

QRectF LogicalOutput::geometryF() const
{
    return QRectF(m_state.position, QSizeF(pixelSize()) / scale());
}

QSize LogicalOutput::physicalSize() const
{
    return m_information.physicalSize;
}

uint32_t LogicalOutput::refreshRate() const
{
    return m_state.currentMode ? m_state.currentMode->refreshRate() : 0;
}

QSize LogicalOutput::modeSize() const
{
    return m_state.currentMode ? m_state.currentMode->size() : QSize();
}

QSize LogicalOutput::pixelSize() const
{
    return orientateSize(modeSize());
}

const Edid &LogicalOutput::edid() const
{
    return m_information.edid;
}

QList<std::shared_ptr<OutputMode>> LogicalOutput::modes() const
{
    return m_state.modes;
}

std::shared_ptr<OutputMode> LogicalOutput::currentMode() const
{
    return m_state.currentMode;
}

QSize LogicalOutput::desiredModeSize() const
{
    return m_state.desiredModeSize;
}

uint32_t LogicalOutput::desiredModeRefreshRate() const
{
    return m_state.desiredModeRefreshRate;
}

LogicalOutput::SubPixel LogicalOutput::subPixel() const
{
    return m_information.subPixel;
}

void LogicalOutput::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    if (!props) {
        return;
    }
    Q_EMIT aboutToChange(props.get());

    State next = m_state;
    next.enabled = props->enabled.value_or(m_state.enabled);
    next.transform = props->transform.value_or(m_state.transform);
    next.position = props->pos.value_or(m_state.position);
    next.scale = props->scale.value_or(m_state.scale);
    next.rgbRange = props->rgbRange.value_or(m_state.rgbRange);
    next.autoRotatePolicy = props->autoRotationPolicy.value_or(m_state.autoRotatePolicy);
    next.iccProfilePath = props->iccProfilePath.value_or(m_state.iccProfilePath);
    if (props->iccProfilePath) {
        next.iccProfile = IccProfile::load(*props->iccProfilePath).value_or(nullptr);
    }
    next.vrrPolicy = props->vrrPolicy.value_or(m_state.vrrPolicy);
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);

    setState(next);

    Q_EMIT changed();
}

bool LogicalOutput::isEnabled() const
{
    return m_state.enabled;
}

QString LogicalOutput::description() const
{
    return manufacturer() + ' ' + model();
}

void LogicalOutput::setInformation(const Information &information)
{
    const auto oldInfo = m_information;
    m_information = information;
    if (oldInfo.capabilities != information.capabilities) {
        Q_EMIT capabilitiesChanged();
    }
}

void LogicalOutput::setState(const State &state)
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
    if (oldState.highDynamicRange != state.highDynamicRange) {
        Q_EMIT highDynamicRangeChanged();
    }
    if (oldState.referenceLuminance != state.referenceLuminance) {
        Q_EMIT referenceLuminanceChanged();
    }
    if (oldState.wideColorGamut != state.wideColorGamut) {
        Q_EMIT wideColorGamutChanged();
    }
    if (oldState.autoRotatePolicy != state.autoRotatePolicy) {
        Q_EMIT autoRotationPolicyChanged();
    }
    if (oldState.iccProfile != state.iccProfile) {
        Q_EMIT iccProfileChanged();
    }
    if (oldState.iccProfilePath != state.iccProfilePath) {
        Q_EMIT iccProfilePathChanged();
    }
    if (oldState.maxPeakBrightnessOverride != state.maxPeakBrightnessOverride
        || oldState.maxAverageBrightnessOverride != state.maxAverageBrightnessOverride
        || oldState.minBrightnessOverride != state.minBrightnessOverride) {
        Q_EMIT brightnessMetadataChanged();
    }
    if (oldState.sdrGamutWideness != state.sdrGamutWideness) {
        Q_EMIT sdrGamutWidenessChanged();
    }
    if (oldState.vrrPolicy != state.vrrPolicy) {
        Q_EMIT vrrPolicyChanged();
    }
    if (oldState.colorDescription != state.colorDescription) {
        Q_EMIT colorDescriptionChanged();
    }
    if (oldState.colorProfileSource != state.colorProfileSource) {
        Q_EMIT colorProfileSourceChanged();
    }
    if (oldState.brightnessSetting != state.brightnessSetting) {
        Q_EMIT brightnessChanged();
    }
    if (oldState.colorPowerTradeoff != state.colorPowerTradeoff) {
        Q_EMIT colorPowerTradeoffChanged();
    }
    if (oldState.dimming != state.dimming) {
        Q_EMIT dimmingChanged();
    }
    if (oldState.uuid != state.uuid) {
        Q_EMIT uuidChanged();
    }
    if (oldState.replicationSource != state.replicationSource) {
        Q_EMIT replicationSourceChanged();
    }
    // detectedDdcCi is ignored here, it should result in capabilitiesChanged() instead
    if (oldState.allowDdcCi != state.allowDdcCi) {
        Q_EMIT allowDdcCiChanged();
    }
    if (oldState.maxBitsPerColor != state.maxBitsPerColor
        || oldState.automaticMaxBitsPerColorLimit != state.automaticMaxBitsPerColorLimit) {
        Q_EMIT maxBitsPerColorChanged();
    }
    if (oldState.edrPolicy != state.edrPolicy) {
        Q_EMIT edrPolicyChanged();
    }
    if (oldState.blendingColor != state.blendingColor) {
        Q_EMIT blendingColorChanged();
    }
    if (oldState.enabled != state.enabled) {
        Q_EMIT enabledChanged();
    }
}

QSize LogicalOutput::orientateSize(const QSize &size) const
{
    switch (m_state.transform.kind()) {
    case OutputTransform::Rotate90:
    case OutputTransform::Rotate270:
    case OutputTransform::FlipX90:
    case OutputTransform::FlipX270:
        return size.transposed();
    default:
        return size;
    }
}

void LogicalOutput::setDpmsMode(DpmsMode mode)
{
}

LogicalOutput::DpmsMode LogicalOutput::dpmsMode() const
{
    return m_state.dpmsMode;
}

uint32_t LogicalOutput::overscan() const
{
    return m_state.overscan;
}

VrrPolicy LogicalOutput::vrrPolicy() const
{
    return m_state.vrrPolicy;
}

bool LogicalOutput::isPlaceholder() const
{
    return m_information.placeholder;
}

bool LogicalOutput::isNonDesktop() const
{
    return m_information.nonDesktop;
}

LogicalOutput::RgbRange LogicalOutput::rgbRange() const
{
    return m_state.rgbRange;
}

bool LogicalOutput::setChannelFactors(const QVector3D &rgb)
{
    return false;
}

OutputTransform LogicalOutput::panelOrientation() const
{
    return m_information.panelOrientation;
}

bool LogicalOutput::wideColorGamut() const
{
    return m_state.wideColorGamut;
}

bool LogicalOutput::highDynamicRange() const
{
    return m_state.highDynamicRange;
}

uint32_t LogicalOutput::referenceLuminance() const
{
    return m_state.referenceLuminance;
}

LogicalOutput::AutoRotationPolicy LogicalOutput::autoRotationPolicy() const
{
    return m_state.autoRotatePolicy;
}

std::shared_ptr<IccProfile> LogicalOutput::iccProfile() const
{
    return m_state.iccProfile;
}

QString LogicalOutput::iccProfilePath() const
{
    return m_state.iccProfilePath;
}

QByteArray LogicalOutput::mstPath() const
{
    return m_information.mstPath;
}

const ColorDescription &LogicalOutput::colorDescription() const
{
    return m_state.colorDescription;
}

std::optional<double> LogicalOutput::maxPeakBrightness() const
{
    return m_state.maxPeakBrightnessOverride ? m_state.maxPeakBrightnessOverride : m_information.maxPeakBrightness;
}

std::optional<double> LogicalOutput::maxAverageBrightness() const
{
    return m_state.maxAverageBrightnessOverride ? *m_state.maxAverageBrightnessOverride : m_information.maxAverageBrightness;
}

double LogicalOutput::minBrightness() const
{
    return m_state.minBrightnessOverride.value_or(m_information.minBrightness);
}

std::optional<double> LogicalOutput::maxPeakBrightnessOverride() const
{
    return m_state.maxPeakBrightnessOverride;
}

std::optional<double> LogicalOutput::maxAverageBrightnessOverride() const
{
    return m_state.maxAverageBrightnessOverride;
}

std::optional<double> LogicalOutput::minBrightnessOverride() const
{
    return m_state.minBrightnessOverride;
}

double LogicalOutput::sdrGamutWideness() const
{
    return m_state.sdrGamutWideness;
}

LogicalOutput::ColorProfileSource LogicalOutput::colorProfileSource() const
{
    return m_state.colorProfileSource;
}

double LogicalOutput::brightnessSetting() const
{
    return m_state.brightnessSetting;
}

double LogicalOutput::dimming() const
{
    return m_state.dimming;
}

std::optional<double> LogicalOutput::currentBrightness() const
{
    return m_state.currentBrightness;
}

double LogicalOutput::artificialHdrHeadroom() const
{
    return m_state.artificialHdrHeadroom;
}

BrightnessDevice *LogicalOutput::brightnessDevice() const
{
    return m_state.brightnessDevice;
}

void LogicalOutput::unsetBrightnessDevice()
{
    State next;
    next.brightnessDevice = nullptr;
    setState(next);
}

bool LogicalOutput::allowSdrSoftwareBrightness() const
{
    return m_state.allowSdrSoftwareBrightness;
}

LogicalOutput::ColorPowerTradeoff LogicalOutput::colorPowerTradeoff() const
{
    return m_state.colorPowerTradeoff;
}

QString LogicalOutput::replicationSource() const
{
    return m_state.replicationSource;
}

bool LogicalOutput::detectedDdcCi() const
{
    return m_state.detectedDdcCi;
}

bool LogicalOutput::allowDdcCi() const
{
    return m_state.allowDdcCi;
}

uint32_t LogicalOutput::maxBitsPerColor() const
{
    return m_state.maxBitsPerColor;
}

LogicalOutput::BpcRange LogicalOutput::bitsPerColorRange() const
{
    return m_information.bitsPerColorRange;
}

std::optional<uint32_t> LogicalOutput::automaticMaxBitsPerColorLimit() const
{
    return m_state.automaticMaxBitsPerColorLimit;
}

LogicalOutput::EdrPolicy LogicalOutput::edrPolicy() const
{
    return m_state.edrPolicy;
}

void LogicalOutput::setAutoRotateAvailable(bool isAvailable)
{
}

std::optional<uint32_t> LogicalOutput::minVrrRefreshRateHz() const
{
    return m_information.minVrrRefreshRateHz;
}

bool LogicalOutput::updateCursorLayer(std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    return false;
}

void LogicalOutput::repairPresentation()
{
}

const ColorDescription &LogicalOutput::blendingColor() const
{
    return m_state.blendingColor;
}

const ColorDescription &LogicalOutput::layerBlendingColor() const
{
    return m_state.layerBlendingColor;
}

// TODO move these quirks to libdisplay-info?
static const std::array s_brokenDdcCi = {
    std::make_pair(QByteArrayLiteral("SAM"), QByteArrayLiteral("Odyssey G5")),
};

bool LogicalOutput::isDdcCiKnownBroken() const
{
    return m_information.edid.isValid() && std::ranges::any_of(s_brokenDdcCi, [this](const auto &pair) {
        return m_information.edid.eisaId() == pair.first
            && m_information.edid.monitorName() == pair.second;
    });
}

} // namespace KWin

#include "moc_output.cpp"
