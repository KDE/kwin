/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include "core/rect.h"
#include "core/region.h"
#include "renderloop.h"
#include "utils/edid.h"

#include <QDebug>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QSize>
#include <QString>
#include <QUuid>

class TestXdgOutput;
class TestWaylandOutput;

namespace KWin
{

class BackendOutput;
class OutputChangeSet;
class OutputMode;

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

    auto operator<=>(const OutputTransform &other) const = default;

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
    RectF map(const RectF &rect, const QSizeF &bounds) const;
    Rect map(const Rect &rect, const QSize &bounds) const;

    /**
     * Applies the output transform to the given @a point.
     */
    QPointF map(const QPointF &point, const QSizeF &bounds) const;
    QPoint map(const QPoint &point, const QSize &bounds) const;

    /**
     * Applies the output transform to the given @a region
     */
    Region map(const Region &region, const QSize &bounds) const;

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

/**
 * The CvtModeline represents VESA CVT standard timings modelines.
 */
struct KWIN_EXPORT CvtModeline
{
    /**
     * Generates a CVT modeline with the specified @a size and @a refreshRate. The refresh rate
     * is specified in milli-Hertz.
     *
     * If @a reducedBlanking is @c true, the modeline will be generated with a reduced blanking
     * region. The blanking region was originally used by CRT monitors to reset the beam. With LCD
     * screens, it's mostly irrlevant now. This flag can be set to reduce the bandwidth.
     *
     * @note The refresh rate of the generated mode will likely be different from the given @a refreshRate.
     */
    static CvtModeline generate(const QSize &size, uint32_t refreshRate, bool reducedBlanking = false);

    QSize size() const;
    uint32_t refreshRate() const;

    bool operator==(const CvtModeline &other) const = default;

    uint32_t clock;

    uint16_t hdisplay;
    uint16_t hsyncStart;
    uint16_t hsyncEnd;
    uint16_t htotal;
    uint16_t hskew;

    uint16_t vdisplay;
    uint16_t vsyncStart;
    uint16_t vsyncEnd;
    uint16_t vtotal;
    uint16_t vscan;

    uint32_t flags;
};

/**
 * The BasicModeline represents basic output mode timings.
 */
struct KWIN_EXPORT BasicModeline
{
    bool operator==(const BasicModeline &other) const = default;

    QSize size;
    uint32_t refreshRate;
};

using OutputTimings = std::variant<BasicModeline, CvtModeline>;

/**
 * The OutputModeline provides a description about an output mode. The output modeline can be used
 * to save and restore output settings.
 */
class KWIN_EXPORT OutputModeline
{
public:
    enum class Flag : uint {
        Preferred = 0x1,
        Generated = 0x2,
        Custom = 0x8,
        ReducedBlanking = 0x10,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    OutputModeline();
    OutputModeline(const QSize &size, uint32_t refreshRate, Flags flags = {});
    OutputModeline(const OutputTimings &timings, Flags flags = {});

    bool operator==(const OutputModeline &other) const = default;

    bool isEmpty() const;
    QSize size() const;
    uint32_t refreshRate() const;
    Flags flags() const;

    std::optional<CvtModeline> cvt() const;
    std::optional<BasicModeline> basic() const;

    OutputModeline withFlags(Flags flags) const;

    std::shared_ptr<OutputMode> match(const QList<std::shared_ptr<OutputMode>> &modes) const;

private:
    OutputTimings m_timings;
    Flags m_flags;
};

class KWIN_EXPORT OutputMode
{
public:
    explicit OutputMode(const OutputModeline &modeline);
    virtual ~OutputMode() = default;

    QSize size() const;
    uint32_t refreshRate() const;
    OutputModeline::Flags flags() const;
    OutputModeline modeline() const;

    bool isRemoved() const;
    void setRemoved();

private:
    const OutputModeline m_modeline;
    bool m_removed = false;
};

/*!
 * \qmltype LogicalOutput
 * \inqmlmodule org.kde.kwin
 * \nativetype KWin::LogicalOutput
 *
 * \brief Generic output representation for window management purposes
 *
 * This type cannot be created from QML.
 */

/*!
 * \class KWin::LogicalOutput
 * \inmodule KWin
 * \inheaderfile core/output.h
 *
 * \brief Generic output representation for window management purposes
 */
class KWIN_EXPORT LogicalOutput : public QObject
{
    Q_OBJECT

    /*!
     * \qmlproperty Rect LogicalOutput::geometry
     *
     * Returns geometry of this output in device independent pixels
     */

    /*!
     * \property KWin::LogicalOutput::geometry
     *
     * Returns geometry of this output in device independent pixels
     */
    Q_PROPERTY(KWin::Rect geometry READ geometry NOTIFY geometryChanged)

    /*!
     * \qmlproperty real LogicalOutput::devicePixelRatio
     *
     * The ratio between physical pixels and logical pixels
     */

    /*!
     * \property KWin::LogicalOutput::devicePixelRatio
     *
     * The ratio between physical pixels and logical pixels
     */
    Q_PROPERTY(qreal devicePixelRatio READ scale NOTIFY scaleChanged)

    /*!
     * \qmlproperty string LogicalOutput::name
     *
     * A short identifiable name of this output
     */

    /*!
     * \property KWin::LogicalOutput::name
     *
     * A short identifiable name of this output
     */
    Q_PROPERTY(QString name READ name CONSTANT)

    /*!
     * \qmlproperty string LogicalOutput::manufacturer
     *
     * The name of this output's manufacturer
     */

    /*!
     * \property KWin::LogicalOutput::manufacturer
     *
     * The name of this output's manufacturer
     */
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)

    /*!
     * \qmlproperty string LogicalOutput::model
     *
     * The model name for this output
     */

    /*!
     * \property KWin::LogicalOutput::model
     *
     * The model name for this output
     */
    Q_PROPERTY(QString model READ model CONSTANT)

    /*!
     * \qmlproperty string LogicalOutput::serialNumber
     *
     * The serial number for this output
     */

    /*!
     * \property KWin::LogicalOutput::serialNumber
     *
     * The serial number for this output
     */
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)

public:
    explicit LogicalOutput(BackendOutput *backendOutput);
    ~LogicalOutput() override;

    void ref();

    void unref();

    /*!
     * Maps the specified \a rect from the global coordinate system to the output-local coords.
     */
    Rect mapFromGlobal(const Rect &rect) const;

    /*!
     * Maps the specified \a rect from the global coordinate system to the output-local coords.
     */
    RectF mapFromGlobal(const RectF &rect) const;

    /*!
     * Maps a \a rect in this output coordinates to the global coordinate system.
     */
    RectF mapToGlobal(const RectF &rect) const;

    /*!
     * Maps a \a region in this output coordinates to the global coordinate system.
     */
    Region mapToGlobal(const Region &region) const;

    /*!
     * \qmlmethod LogicalOutput::mapToGlobal(point pos)
     *
     * Maps a \a pos in this output coordinates to the global coordinate system.
     */

    /*!
     * Maps a \a pos in this output coordinates to the global coordinate system.
     */
    Q_INVOKABLE QPointF mapToGlobal(const QPointF &pos) const;

    /*!
     * \qmlmethod LogicalOutput::mapFromGlobal(point pos)
     *
     * Maps the specified \a pos from the global coordinate system to the output-local coords
     */

    /*!
     * Maps the specified \a pos from the global coordinate system to the output-local coords
     */
    Q_INVOKABLE QPointF mapFromGlobal(const QPointF &pos) const;

    QString name() const;
    QString description() const;
    QString manufacturer() const;
    QString model() const;
    QString serialNumber() const;
    QString uuid() const;

    Rect geometry() const;

    /*!
     * Returns geometry of this output in device independent pixels, without rounding
     */
    RectF geometryF() const;

    /*!
     * Equivalent to Rect(QPoint(0, 0), geometry().size())
     */
    Rect rect() const;

    /*!
     * Equivalent to RectF(QPointF(0, 0), geometryF().size())
     */
    RectF rectF() const;

    /*!
     * Returns the refresh rate of the output, in milli-Hertz
     */
    uint32_t refreshRate() const;

    /*!
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     */
    bool isInternal() const;

    /*!
     * Returns the ratio between physical pixels and logical pixels.
     */
    qreal scale() const;

    /*!
     * Returns the non-rotated physical size of this output, in millimeters.
     */
    QSize physicalSize() const;

    /*!
     * Returns the resolution of the output.
     */
    QSize pixelSize() const;

    /*!
     * Returns the native resolution of the output, unaffected by the output transform
     */
    QSize modeSize() const;

    /*!
     * Returns the output transform
     */
    OutputTransform transform() const;

    /*!
     * Orientates the \a size according to the current output transform
     */
    QSize orientateSize(const QSize &size) const;

    /*!
     * Returns \c true if this is a placeholder output; otherwise returns \c false.
     *
     * A placeholder output is created when there are no any physical outputs.
     */
    bool isPlaceholder() const;

    /*!
     * The color space in which the scene is blended
     */
    const std::shared_ptr<ColorDescription> &blendingColor() const;

    /*!
     * Returns the associated physical/backend output device
     */
    BackendOutput *backendOutput() const;

Q_SIGNALS:
    /*!
     * This signal is emitted when the geometry of this output has changed.
     */
    void geometryChanged();
    /*!
     * This signal is emitted when the device pixel ratio of the output has changed.
     */
    void scaleChanged();

    /*!
     * Notifies that the output is about to change configuration based on a
     * user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void aboutToChange(OutputChangeSet *changeSet);

    /*!
     * Notifies that the output changed based on a user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void changed();

    /*!
     * This signal is emitted when the blending color description changes
     */
    void blendingColorChanged();

    /*!
     * This signal is emitted when the output transform changes, e.g. the output gets rotated, etc
     */
    void transformChanged();

    /*!
     * This signal is emitted when either modeSize or refreshRate change
     */
    void currentModeChanged();

    void descriptionChanged();

protected:
    BackendOutput *const m_backendOutput;
    int m_refCount = 1;

    // only Workspace is meant to change these properties
    friend class Workspace;
    void setData(const QPoint logicalPosition, const QSize &modeSize,
                 uint32_t refreshRate, OutputTransform transform, double scale,
                 const QString &description);
    // and autotests... TODO make them integration tests instead!
    friend class ::TestXdgOutput;
    friend class ::TestWaylandOutput;
    void copyInfoFrom(BackendOutput *output);

    RectF m_geometry;
    QSize m_modeSize;
    uint32_t m_refreshRate = 60000;
    OutputTransform m_transform;
    double m_scale = 1.0;
    QString m_description;
};

inline Rect LogicalOutput::rect() const
{
    return Rect(QPoint(0, 0), geometry().size());
}

inline RectF LogicalOutput::rectF() const
{
    return RectF(QPointF(0, 0), geometryF().size());
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const LogicalOutput *output);

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::OutputModeline::Flags)
