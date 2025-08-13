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
            debug << ", manufacturer=" << output->backendOutput()->manufacturer();
            debug << ", model=" << output->backendOutput()->model();
            debug << ", serialNumber=" << output->backendOutput()->serialNumber();
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

QRegion OutputTransform::map(const QRegion &region, const QSize &bounds) const
{
    QRegion ret;
    for (const QRect &rect : region) {
        ret |= map(rect, bounds);
    }
    return ret;
}

LogicalOutput::LogicalOutput(BackendOutput *backendOutput)
    : m_backendOutput(backendOutput)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    connect(backendOutput, &BackendOutput::positionChanged, this, &LogicalOutput::geometryChanged);
    connect(backendOutput, &BackendOutput::currentModeChanged, this, &LogicalOutput::geometryChanged);
    connect(backendOutput, &BackendOutput::transformChanged, this, &LogicalOutput::geometryChanged);
    connect(backendOutput, &BackendOutput::scaleChanged, this, &LogicalOutput::geometryChanged);
    connect(backendOutput, &BackendOutput::scaleChanged, this, &LogicalOutput::scaleChanged);
    // TODO dpms being kind of on the backend output and kind of here isn't great
    connect(backendOutput, &BackendOutput::aboutToChange, this, &LogicalOutput::aboutToChange);
    connect(backendOutput, &BackendOutput::changed, this, &LogicalOutput::changed);
    connect(backendOutput, &BackendOutput::blendingColorChanged, this, &LogicalOutput::blendingColorChanged);
    connect(backendOutput, &BackendOutput::transformChanged, this, &LogicalOutput::transformChanged);
    connect(backendOutput, &BackendOutput::currentModeChanged, this, &LogicalOutput::currentModeChanged);
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

qreal LogicalOutput::scale() const
{
    return m_backendOutput->scale();
}

QRect LogicalOutput::geometry() const
{
    return QRect(m_backendOutput->position(), m_backendOutput->pixelSize() / scale());
}

QRectF LogicalOutput::geometryF() const
{
    return QRectF(m_backendOutput->position(), QSizeF(m_backendOutput->pixelSize()) / scale());
}

QSize LogicalOutput::modeSize() const
{
    return m_backendOutput->modeSize();
}

QSize LogicalOutput::pixelSize() const
{
    return orientateSize(modeSize());
}

QSize LogicalOutput::orientateSize(const QSize &size) const
{
    switch (transform().kind()) {
    case OutputTransform::Rotate90:
    case OutputTransform::Rotate270:
    case OutputTransform::FlipX90:
    case OutputTransform::FlipX270:
        return size.transposed();
    default:
        return size;
    }
}

BackendOutput *LogicalOutput::backendOutput() const
{
    return m_backendOutput;
}

QString LogicalOutput::name() const
{
    return m_backendOutput->name();
}

QString LogicalOutput::description() const
{
    return m_backendOutput->description();
}

QString LogicalOutput::manufacturer() const
{
    return m_backendOutput->manufacturer();
}

QString LogicalOutput::model() const
{
    return m_backendOutput->model();
}

QString LogicalOutput::serialNumber() const
{
    return m_backendOutput->serialNumber();
}

QString LogicalOutput::uuid() const
{
    return m_backendOutput->uuid();
}

bool LogicalOutput::isPlaceholder() const
{
    return m_backendOutput->isPlaceholder();
}

QSize LogicalOutput::physicalSize() const
{
    return m_backendOutput->physicalSize();
}

const std::shared_ptr<ColorDescription> &LogicalOutput::blendingColor() const
{
    return m_backendOutput->blendingColor();
}

OutputTransform LogicalOutput::transform() const
{
    return m_backendOutput->transform();
}

bool LogicalOutput::isInternal() const
{
    return m_backendOutput->isInternal();
}

uint32_t LogicalOutput::refreshRate() const
{
    return m_backendOutput->refreshRate();
}

} // namespace KWin

#include "moc_output.cpp"
