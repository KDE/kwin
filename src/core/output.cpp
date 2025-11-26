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

RectF OutputTransform::map(const RectF &rect, const QSizeF &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return rect;
    case Kind::Rotate90:
        return RectF(QPointF(rect.top(), bounds.width() - rect.right()),
                     QPointF(rect.bottom(), bounds.width() - rect.left()));
    case Kind::Rotate180:
        return RectF(QPointF(bounds.width() - rect.right(), bounds.height() - rect.bottom()),
                     QPointF(bounds.width() - rect.left(), bounds.height() - rect.top()));
    case Kind::Rotate270:
        return RectF(QPointF(bounds.height() - rect.bottom(), rect.left()),
                     QPointF(bounds.height() - rect.top(), rect.right()));
    case Kind::FlipX:
        return RectF(QPointF(bounds.width() - rect.right(), rect.top()),
                     QPointF(bounds.width() - rect.left(), rect.bottom()));
    case Kind::FlipX90:
        return RectF(QPointF(rect.top(), rect.left()),
                     QPointF(rect.bottom(), rect.right()));
    case Kind::FlipX180:
        return RectF(QPointF(rect.left(), bounds.height() - rect.bottom()),
                     QPointF(rect.right(), bounds.height() - rect.top()));
    case Kind::FlipX270:
        return RectF(QPointF(bounds.height() - rect.bottom(), bounds.width() - rect.right()),
                     QPointF(bounds.height() - rect.top(), bounds.width() - rect.left()));
    default:
        Q_UNREACHABLE();
    }
}

Rect OutputTransform::map(const Rect &rect, const QSize &bounds) const
{
    switch (m_kind) {
    case Kind::Normal:
        return rect;
    case Kind::Rotate90:
        return Rect(QPoint(rect.top(), bounds.width() - rect.right()),
                    QPoint(rect.bottom(), bounds.width() - rect.left()));
    case Kind::Rotate180:
        return Rect(QPoint(bounds.width() - rect.right(), bounds.height() - rect.bottom()),
                    QPoint(bounds.width() - rect.left(), bounds.height() - rect.top()));
    case Kind::Rotate270:
        return Rect(QPoint(bounds.height() - rect.bottom(), rect.left()),
                    QPoint(bounds.height() - rect.top(), rect.right()));
    case Kind::FlipX:
        return Rect(QPoint(bounds.width() - rect.right(), rect.top()),
                    QPoint(bounds.width() - rect.left(), rect.bottom()));
    case Kind::FlipX90:
        return Rect(QPoint(rect.top(), rect.left()),
                    QPoint(rect.bottom(), rect.right()));
    case Kind::FlipX180:
        return Rect(QPoint(rect.left(), bounds.height() - rect.bottom()),
                    QPoint(rect.right(), bounds.height() - rect.top()));
    case Kind::FlipX270:
        return Rect(QPoint(bounds.height() - rect.bottom(), bounds.width() - rect.right()),
                    QPoint(bounds.height() - rect.top(), bounds.width() - rect.left()));
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

Region OutputTransform::map(const Region &region, const QSize &bounds) const
{
    Region ret;
    for (const Rect &rect : region.rects()) {
        ret |= map(rect, bounds);
    }
    return ret;
}

LogicalOutput::LogicalOutput(BackendOutput *backendOutput)
    : m_backendOutput(backendOutput)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    connect(backendOutput, &BackendOutput::blendingColorChanged, this, &LogicalOutput::blendingColorChanged);
    connect(backendOutput, &BackendOutput::currentModeChanged, this, &LogicalOutput::currentModeChanged);
    // Workspace will emit "changed" when all the properties have been updated
    connect(backendOutput, &BackendOutput::aboutToChange, this, &LogicalOutput::aboutToChange);
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

Rect LogicalOutput::mapFromGlobal(const Rect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

RectF LogicalOutput::mapFromGlobal(const RectF &rect) const
{
    return rect.translated(-geometry().topLeft());
}

RectF LogicalOutput::mapToGlobal(const RectF &rect) const
{
    return rect.translated(geometry().topLeft());
}

Region LogicalOutput::mapToGlobal(const Region &region) const
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
    return m_scale;
}

Rect LogicalOutput::geometry() const
{
    return m_geometry.rounded();
}

RectF LogicalOutput::geometryF() const
{
    return m_geometry;
}

QSize LogicalOutput::modeSize() const
{
    return m_modeSize;
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
    return m_transform;
}

bool LogicalOutput::isInternal() const
{
    return m_backendOutput->isInternal();
}

uint32_t LogicalOutput::refreshRate() const
{
    return m_backendOutput->refreshRate();
}

void LogicalOutput::setGeometry(const QPoint logicalPosition, const QSize &modeSize, OutputTransform transform, double scale)
{
    m_modeSize = modeSize;
    const RectF newGeometry(logicalPosition, transform.map(QSizeF(modeSize)) / scale);
    if (newGeometry != m_geometry) {
        m_geometry = newGeometry;
        Q_EMIT geometryChanged();
    }
    if (m_transform != transform) {
        m_transform = transform;
        Q_EMIT transformChanged();
    }
}

} // namespace KWin

#include "moc_output.cpp"
