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
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QUuid>

namespace KWin
{

class RenderLoop;
class OutputConfiguration;
class ColorTransformation;
class IccProfile;

enum class ContentType {
    None = 0,
    Photo = 1,
    Video = 2,
    Game = 3,
};

/**
 * The OutputTransform type is used to describe the transform applied to the output content.
 * Rotation is clockwise.
 */
class KWIN_EXPORT OutputTransform
{
public:
    enum Kind {
        Normal,
        Rotated90,
        Rotated180,
        Rotated270,
        Flipped,
        Flipped90,
        Flipped180,
        Flipped270
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

    /**
     * Applies the output transform to the given @a rect within a buffer with dimensions @a bounds.
     */
    QRectF map(const QRectF &rect, const QSizeF &bounds) const;

private:
    Kind m_kind = Kind::Normal;
};

class KWIN_EXPORT OutputMode
{
public:
    enum class Flag : uint {
        Preferred = 0x1,
        Generated = 0x2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    OutputMode(const QSize &size, uint32_t refreshRate, Flags flags = {});
    virtual ~OutputMode() = default;

    QSize size() const;
    uint32_t refreshRate() const;
    Flags flags() const;

private:
    const QSize m_size;
    const uint32_t m_refreshRate;
    const Flags m_flags;
};

/**
 * Generic output representation.
 */
class KWIN_EXPORT Output : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(qreal devicePixelRatio READ scale NOTIFY scaleChanged)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)
    Q_PROPERTY(QString model READ model CONSTANT)
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)

public:
    enum class DpmsMode {
        On,
        Standby,
        Suspend,
        Off,
    };
    Q_ENUM(DpmsMode)

    enum class Capability : uint {
        Dpms = 1,
        Overscan = 1 << 1,
        Vrr = 1 << 2,
        RgbRange = 1 << 3,
        HighDynamicRange = 1 << 4,
        WideColorGamut = 1 << 5,
        AutoRotation = 1 << 6,
        IccProfile = 1 << 7,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum class SubPixel {
        Unknown,
        None,
        Horizontal_RGB,
        Horizontal_BGR,
        Vertical_RGB,
        Vertical_BGR,
    };
    Q_ENUM(SubPixel)

    enum class RgbRange {
        Automatic = 0,
        Full = 1,
        Limited = 2,
    };
    Q_ENUM(RgbRange)

    enum class AutoRotationPolicy {
        Never = 0,
        InTabletMode,
        Always
    };
    Q_ENUM(AutoRotationPolicy);

    explicit Output(QObject *parent = nullptr);
    ~Output() override;

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

    Q_INVOKABLE QPointF mapToGlobal(const QPointF &pos) const;
    Q_INVOKABLE QPointF mapFromGlobal(const QPointF &pos) const;

    /**
     * Returns a short identifiable name of this output.
     */
    QString name() const;

    /**
     * Returns the identifying uuid of this output.
     */
    QUuid uuid() const;

    /**
     * Returns @c true if the output is enabled; otherwise returns @c false.
     */
    bool isEnabled() const;

    /**
     * Returns geometry of this output in device independent pixels.
     */
    QRect geometry() const;

    /**
     * Returns geometry of this output in device independent pixels, without rounding
     */
    QRectF fractionalGeometry() const;

    /**
     * Equivalent to `QRect(QPoint(0, 0), geometry().size())`
     */
    QRect rect() const;

    /**
     * Returns the approximate vertical refresh rate of this output, in mHz.
     */
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

    QString eisaId() const;

    /**
     * Returns the manufacturer of the screen.
     */
    QString manufacturer() const;
    /**
     * Returns the model of the screen.
     */
    QString model() const;
    /**
     * Returns the serial number of the screen.
     */
    QString serialNumber() const;

    /**
     * Returns the RenderLoop for this output. If the platform does not support per screen
     * rendering, all outputs will share the same render loop.
     */
    virtual RenderLoop *renderLoop() const = 0;

    void inhibitDirectScanout();
    void uninhibitDirectScanout();

    bool directScanoutInhibited() const;

    /**
     * @returns the configured time for an output to dim
     *
     * This allows the backends to coordinate with the front-end the time they
     * allow to decorate the dimming until the display is turned off
     *
     * @see aboutToTurnOff
     */
    static std::chrono::milliseconds dimAnimationTime();

    OutputTransform transform() const;
    /**
     * The transform that the user has configured, and which doesn't get changed
     * by automatic rotation
     */
    OutputTransform manualTransform() const;
    QSize orientateSize(const QSize &size) const;

    void applyChanges(const OutputConfiguration &config);

    SubPixel subPixel() const;
    QString description() const;
    Capabilities capabilities() const;
    const Edid &edid() const;
    QList<std::shared_ptr<OutputMode>> modes() const;
    std::shared_ptr<OutputMode> currentMode() const;
    DpmsMode dpmsMode() const;
    virtual void setDpmsMode(DpmsMode mode);

    uint32_t overscan() const;

    /**
     * Returns a matrix that can translate into the display's coordinates system
     */
    static QMatrix4x4 logicalToNativeMatrix(const QRectF &rect, qreal scale, OutputTransform transform);

    void setVrrPolicy(RenderLoop::VrrPolicy policy);
    RenderLoop::VrrPolicy vrrPolicy() const;
    RgbRange rgbRange() const;

    ContentType contentType() const;
    void setContentType(ContentType contentType);

    bool isPlaceholder() const;
    bool isNonDesktop() const;
    OutputTransform panelOrientation() const;
    bool wideColorGamut() const;
    bool highDynamicRange() const;
    uint32_t sdrBrightness() const;
    AutoRotationPolicy autoRotationPolicy() const;
    std::shared_ptr<IccProfile> iccProfile() const;
    QString iccProfilePath() const;
    /**
     * @returns the mst path of this output. Is empty if invalid
     */
    QByteArray mstPath() const;

    virtual bool setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation);
    virtual bool setChannelFactors(const QVector3D &rgb);

    virtual bool updateCursorLayer();

Q_SIGNALS:
    /**
     * This signal is emitted when the geometry of this output has changed.
     */
    void geometryChanged();
    /**
     * This signal is emitted when the output has been enabled or disabled.
     */
    void enabledChanged();
    /**
     * This signal is emitted when the device pixel ratio of the output has changed.
     */
    void scaleChanged();

    /**
     * Notifies that the display will be dimmed in @p time ms. This allows
     * effects to plan for it and hopefully animate it
     */
    void aboutToTurnOff(std::chrono::milliseconds time);

    /**
     * Notifies that the output has been turned on and the wake can be decorated.
     */
    void wakeUp();

    /**
     * Notifies that the output is about to change configuration based on a
     * user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void aboutToChange();

    /**
     * Notifies that the output changed based on a user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void changed();

    void currentModeChanged();
    void modesChanged();
    void outputChange(const QRegion &damagedRegion);
    void transformChanged();
    void dpmsModeChanged();
    void capabilitiesChanged();
    void overscanChanged();
    void vrrPolicyChanged();
    void rgbRangeChanged();
    void wideColorGamutChanged();
    void sdrBrightnessChanged();
    void highDynamicRangeChanged();
    void autoRotationPolicyChanged();
    void iccProfileChanged();
    void iccProfilePathChanged();

protected:
    struct Information
    {
        QString name;
        QString manufacturer;
        QString model;
        QString serialNumber;
        QString eisaId;
        QSize physicalSize;
        Edid edid;
        SubPixel subPixel = SubPixel::Unknown;
        Capabilities capabilities;
        OutputTransform panelOrientation = OutputTransform::Normal;
        bool internal = false;
        bool placeholder = false;
        bool nonDesktop = false;
        QByteArray mstPath;
    };

    struct State
    {
        QPoint position;
        qreal scale = 1;
        OutputTransform transform = OutputTransform::Normal;
        OutputTransform manualTransform = OutputTransform::Normal;
        QList<std::shared_ptr<OutputMode>> modes;
        std::shared_ptr<OutputMode> currentMode;
        DpmsMode dpmsMode = DpmsMode::On;
        SubPixel subPixel = SubPixel::Unknown;
        bool enabled = false;
        uint32_t overscan = 0;
        RgbRange rgbRange = RgbRange::Automatic;
        bool wideColorGamut = false;
        bool highDynamicRange = false;
        uint32_t sdrBrightness = 200;
        AutoRotationPolicy autoRotatePolicy = AutoRotationPolicy::InTabletMode;
        QString iccProfilePath;
        std::shared_ptr<IccProfile> iccProfile;
    };

    void setInformation(const Information &information);
    void setState(const State &state);

    State m_state;
    Information m_information;
    QUuid m_uuid;
    int m_directScanoutCount = 0;
    int m_refCount = 1;
    ContentType m_contentType = ContentType::None;
};

inline QRect Output::rect() const
{
    return QRect(QPoint(0, 0), geometry().size());
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const Output *output);

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Output::Capabilities)
