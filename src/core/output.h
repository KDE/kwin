/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include "renderloop.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QUuid>
#include <QVector>

namespace KWin
{

class CursorSource;
class EffectScreenImpl;
class RenderLoop;
class OutputConfiguration;
class ColorTransformation;

enum class ContentType {
    None = 0,
    Photo = 1,
    Video = 2,
    Game = 3,
};

class KWIN_EXPORT OutputMode
{
public:
    enum class Flag : uint {
        Preferred = 0x1,
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

public:
    enum class DpmsMode {
        On,
        Standby,
        Suspend,
        Off,
    };
    Q_ENUM(DpmsMode)

    enum class Capability : uint {
        Dpms = 0x1,
        Overscan = 0x2,
        Vrr = 0x4,
        RgbRange = 0x8,
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

    /**
     * Returns a short identifiable name of this output.
     */
    QString name() const;

    /**
     * Returns the identifying uuid of this output.
     *
     * Default implementation returns an empty byte array.
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
    int refreshRate() const;

    /**
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     */
    bool isInternal() const;

    /**
     * Returns the ratio between physical pixels and logical pixels.
     *
     * Default implementation returns 1.
     */
    qreal scale() const;

    /**
     * Returns the non-rotated physical size of this output, in millimeters.
     *
     * Default implementation returns an invalid QSize.
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

    enum class Transform {
        Normal,
        Rotated90,
        Rotated180,
        Rotated270,
        Flipped,
        Flipped90,
        Flipped180,
        Flipped270
    };
    Q_ENUM(Transform)
    Transform transform() const;
    QSize orientateSize(const QSize &size) const;

    void applyChanges(const OutputConfiguration &config);

    SubPixel subPixel() const;
    QString description() const;
    Capabilities capabilities() const;
    QByteArray edid() const;
    QList<std::shared_ptr<OutputMode>> modes() const;
    std::shared_ptr<OutputMode> currentMode() const;
    DpmsMode dpmsMode() const;
    virtual void setDpmsMode(DpmsMode mode);

    uint32_t overscan() const;

    /**
     * Returns a matrix that can translate into the display's coordinates system
     */
    static QMatrix4x4 logicalToNativeMatrix(const QRect &rect, qreal scale, Transform transform);

    void setVrrPolicy(RenderLoop::VrrPolicy policy);
    RenderLoop::VrrPolicy vrrPolicy() const;
    RgbRange rgbRange() const;

    ContentType contentType() const;
    void setContentType(ContentType contentType);

    bool isPlaceholder() const;
    bool isNonDesktop() const;
    Transform panelOrientation() const;

    virtual bool setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation);
    virtual bool setCTM(const QMatrix3x3 &ctm);

    virtual bool setCursor(CursorSource *source);
    virtual bool moveCursor(const QPoint &position);

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

protected:
    struct Information
    {
        QString name;
        QString manufacturer;
        QString model;
        QString serialNumber;
        QString eisaId;
        QSize physicalSize;
        QByteArray edid;
        SubPixel subPixel = SubPixel::Unknown;
        Capabilities capabilities;
        Transform panelOrientation = Transform::Normal;
        bool internal = false;
        bool placeholder = false;
        bool nonDesktop = false;
    };

    struct State
    {
        QPoint position;
        qreal scale = 1;
        Transform transform = Transform::Normal;
        QList<std::shared_ptr<OutputMode>> modes;
        std::shared_ptr<OutputMode> currentMode;
        DpmsMode dpmsMode = DpmsMode::On;
        SubPixel subPixel = SubPixel::Unknown;
        bool enabled = false;
        uint32_t overscan = 0;
        RgbRange rgbRange = RgbRange::Automatic;
    };

    void setInformation(const Information &information);
    void setState(const State &state);

    EffectScreenImpl *m_effectScreen = nullptr;
    State m_state;
    Information m_information;
    QUuid m_uuid;
    int m_directScanoutCount = 0;
    int m_refCount = 1;
    ContentType m_contentType = ContentType::None;
    friend class EffectScreenImpl; // to access m_effectScreen
};

inline QRect Output::rect() const
{
    return QRect(QPoint(0, 0), geometry().size());
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const Output *output);

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Output::Capabilities)
