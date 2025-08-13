/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include "renderloop.h"
#include "utils/edid.h"

#include <QDebug>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QUuid>

namespace KWin
{

class BackendOutput;
class OutputChangeSet;

/**
 * The OutputTransform type is used to describe the transform applied to the output content.
 */
class KWIN_EXPORT OutputTransform
{
public:
    enum Kind {
        Normal = 0, // no rotation
        Rotate90 = 1, // rotate 90 degrees counterclockwise
        Rotate180 = 2, // rotate 180 degrees counterclockwise
        Rotate270 = 3, // rotate 270 degrees counterclockwise
        FlipX = 4, // mirror horizontally
        FlipX90 = 5, // mirror horizontally, then rotate 90 degrees counterclockwise
        FlipX180 = 6, // mirror horizontally, then rotate 180 degrees counterclockwise
        FlipX270 = 7, // mirror horizontally, then rotate 270 degrees counterclockwise
        FlipY = FlipX180, // mirror vertically
        FlipY90 = FlipX270, // mirror vertically, then rotate 90 degrees counterclockwise
        FlipY180 = FlipX, // mirror vertically, then rotate 180 degrees counterclockwise
        FlipY270 = FlipX90, // mirror vertically, then rotate 270 degrees counterclockwise
    };

    OutputTransform() = default;
    OutputTransform(Kind kind)
        : m_kind(kind)
    {
    }

    bool operator<=>(const OutputTransform &other) const = default;

    /**
     * Returns the transform kind.
     */
    Kind kind() const;

    /**
     * Returns the inverse transform. The inverse transform can be used for mapping between
     * surface and buffer coordinate systems.
     */
    OutputTransform inverted() const;

    /**
     * Applies the output transform to the given @a size.
     */
    QSizeF map(const QSizeF &size) const;
    QSize map(const QSize &size) const;

    /**
     * Applies the output transform to the given @a rect within a buffer with dimensions @a bounds.
     */
    QRectF map(const QRectF &rect, const QSizeF &bounds) const;
    QRect map(const QRect &rect, const QSize &bounds) const;

    /**
     * Applies the output transform to the given @a point.
     */
    QPointF map(const QPointF &point, const QSizeF &bounds) const;
    QPoint map(const QPoint &point, const QSize &bounds) const;

    /**
     * Applies the output transform to the given @a region
     */
    QRegion map(const QRegion &region, const QSize &bounds) const;

    /**
     * Returns an output transform that is equivalent to applying this transform and @a other
     * transform sequentially.
     */
    OutputTransform combine(OutputTransform other) const;

    /**
     * Returns the matrix corresponding to this output transform.
     */
    QMatrix4x4 toMatrix() const;

private:
    Kind m_kind = Kind::Normal;
};

class KWIN_EXPORT OutputMode
{
public:
    enum class Flag : uint {
        Preferred = 0x1,
        Generated = 0x2,
        Removed = 0x4,
        Custom = 0x8,
        ReducedBlanking = 0x10,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    OutputMode(const QSize &size, uint32_t refreshRate, Flags flags = {});
    virtual ~OutputMode() = default;

    QSize size() const;
    uint32_t refreshRate() const;
    Flags flags() const;

    void setRemoved();

private:
    const QSize m_size;
    const uint32_t m_refreshRate;
    Flags m_flags;
};

struct CustomModeDefinition
{
    QSize size;
    uint32_t refreshRate;
    OutputMode::Flags flags;
};

/**
 * Generic output representation for window management purposes
 */
class KWIN_EXPORT LogicalOutput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(qreal devicePixelRatio READ scale NOTIFY scaleChanged)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)
    Q_PROPERTY(QString model READ model CONSTANT)
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)

public:
    explicit LogicalOutput(BackendOutput *backendOutput);
    ~LogicalOutput() override;

    void ref();
    void unref();

    /**
     * Maps the specified @a rect from the global coordinate system to the output-local coords.
     */
    QRect mapFromGlobal(const QRect &rect) const;

    /**
     * Maps the specified @a rect from the global coordinate system to the output-local coords.
     */
    QRectF mapFromGlobal(const QRectF &rect) const;

    /**
     * Maps a @a rect in this output coordinates to the global coordinate system.
     */
    QRectF mapToGlobal(const QRectF &rect) const;

    /**
     * Maps a @a region in this output coordinates to the global coordinate system.
     */
    QRegion mapToGlobal(const QRegion &region) const;

    Q_INVOKABLE QPointF mapToGlobal(const QPointF &pos) const;
    Q_INVOKABLE QPointF mapFromGlobal(const QPointF &pos) const;

    /**
     * Returns a short identifiable name of this output.
     */
    QString name() const;
    QString description() const;
    QString manufacturer() const;
    QString model() const;
    QString serialNumber() const;
    QString uuid() const;

    /**
     * Returns geometry of this output in device independent pixels.
     */
    QRect geometry() const;

    /**
     * Returns geometry of this output in device independent pixels, without rounding
     */
    QRectF geometryF() const;

    /**
     * Equivalent to `QRect(QPoint(0, 0), geometry().size())`
     */
    QRect rect() const;

    /**
     * Equivalent to `QRectF(QPointF(0, 0), geometryF().size())`
     */
    QRectF rectF() const;

    uint32_t refreshRate() const;

    /**
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     */
    bool isInternal() const;

    /**
     * Returns the ratio between physical pixels and logical pixels.
     */
    qreal scale() const;

    /**
     * Returns the non-rotated physical size of this output, in millimeters.
     */
    QSize physicalSize() const;

    /** Returns the resolution of the output.  */
    QSize pixelSize() const;
    QSize modeSize() const;
    OutputTransform transform() const;
    QSize orientateSize(const QSize &size) const;

    bool isPlaceholder() const;

    /**
     * The color space in which the scene is blended
     */
    const std::shared_ptr<ColorDescription> &blendingColor() const;

    BackendOutput *backendOutput() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the geometry of this output has changed.
     */
    void geometryChanged();
    /**
     * This signal is emitted when the device pixel ratio of the output has changed.
     */
    void scaleChanged();

    /**
     * Notifies that the output is about to change configuration based on a
     * user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void aboutToChange(OutputChangeSet *changeSet);

    /**
     * Notifies that the output changed based on a user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void changed();

    void blendingColorChanged();
    void transformChanged();
    /**
     * This signal is emitted when either modeSize or refreshRate change
     */
    void currentModeChanged();

protected:
    BackendOutput *const m_backendOutput;
    int m_refCount = 1;
};

inline QRect LogicalOutput::rect() const
{
    return QRect(QPoint(0, 0), geometry().size());
}

inline QRectF LogicalOutput::rectF() const
{
    return QRectF(QPointF(0, 0), geometryF().size());
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const LogicalOutput *output);

} // namespace KWin
