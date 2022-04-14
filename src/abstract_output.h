/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_OUTPUT_H
#define KWIN_ABSTRACT_OUTPUT_H

#include <kwin_export.h>

#include "renderloop.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QUuid>
#include <QVector>

namespace KWin
{

class EffectScreenImpl;
class RenderLoop;
class OutputConfiguration;

class KWIN_EXPORT GammaRamp
{
public:
    GammaRamp(uint32_t size);

    /**
     * Returns the size of the gamma ramp.
     */
    uint32_t size() const;

    /**
     * Returns pointer to the first red component in the gamma ramp.
     *
     * The returned pointer can be used for altering the red component
     * in the gamma ramp.
     */
    uint16_t *red();

    /**
     * Returns pointer to the first red component in the gamma ramp.
     */
    const uint16_t *red() const;

    /**
     * Returns pointer to the first green component in the gamma ramp.
     *
     * The returned pointer can be used for altering the green component
     * in the gamma ramp.
     */
    uint16_t *green();

    /**
     * Returns pointer to the first green component in the gamma ramp.
     */
    const uint16_t *green() const;

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     *
     * The returned pointer can be used for altering the blue component
     * in the gamma ramp.
     */
    uint16_t *blue();

    /**
     * Returns pointer to the first blue component in the gamma ramp.
     */
    const uint16_t *blue() const;

private:
    QVector<uint16_t> m_table;
    uint32_t m_size;
};

/**
 * Generic output representation.
 */
class KWIN_EXPORT AbstractOutput : public QObject
{
    Q_OBJECT

public:
    enum class ModeFlag : uint {
        Current = 0x1,
        Preferred = 0x2,
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)
    Q_ENUM(ModeFlag)

    struct Mode
    {
        QSize size;
        int refreshRate;
        ModeFlags flags;
        int id;

        inline bool operator==(const Mode &other) const;
    };

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

    explicit AbstractOutput(QObject *parent = nullptr);
    ~AbstractOutput() override;

    /**
     * Maps the specified @a rect from the global coordinate system to the output-local coords.
     */
    QRect mapFromGlobal(const QRect &rect) const;

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
     * Enable or disable the output.
     *
     * Default implementation does nothing
     */
    void setEnabled(bool enable);

    /**
     * Returns geometry of this output in device independent pixels.
     */
    QRect geometry() const;

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
     * Returns the physical size of this output, in millimeters.
     *
     * Default implementation returns an invalid QSize.
     */
    QSize physicalSize() const;

    /**
     * Returns the size of the gamma lookup table.
     *
     * Default implementation returns 0.
     */
    virtual int gammaRampSize() const;

    /**
     * Sets the gamma ramp of this output.
     *
     * Returns @c true if the gamma ramp was successfully set.
     */
    virtual bool setGammaRamp(const GammaRamp &gamma);

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

    virtual bool usesSoftwareCursor() const;

    void moveTo(const QPoint &pos);
    void setScale(qreal scale);

    void applyChanges(const OutputConfiguration &config);

    SubPixel subPixel() const;
    QString description() const;
    Capabilities capabilities() const;
    QByteArray edid() const;
    QVector<Mode> modes() const;
    void setModes(const QVector<Mode> &modes);
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

    bool isPlaceholder() const;

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
    void initialize(const QString &model, const QString &manufacturer,
                    const QString &eisaId, const QString &serialNumber,
                    const QSize &physicalSize,
                    const QVector<Mode> &modes, const QByteArray &edid);

    void setName(const QString &name)
    {
        m_name = name;
    }
    void setInternal(bool set)
    {
        m_internal = set;
    }

    virtual void updateEnablement(bool enable)
    {
        Q_UNUSED(enable);
    }
    virtual void updateTransform(Transform transform)
    {
        Q_UNUSED(transform);
    }

    void setCurrentModeInternal(const QSize &size, int refreshRate);
    void setTransformInternal(Transform transform);
    void setDpmsModeInternal(DpmsMode dpmsMode);
    void setCapabilityInternal(Capability capability, bool on = true);
    void setSubPixelInternal(SubPixel subPixel);
    void setOverscanInternal(uint32_t overscan);
    void setPlaceholder(bool isPlaceholder);
    void setRgbRangeInternal(RgbRange range);
    void setPhysicalSizeInternal(const QSize &size);

    QSize orientateSize(const QSize &size) const;

private:
    Q_DISABLE_COPY(AbstractOutput)
    EffectScreenImpl *m_effectScreen = nullptr;
    int m_directScanoutCount = 0;
    QString m_name;
    QString m_eisaId;
    QString m_manufacturer;
    QString m_model;
    QString m_serialNumber;
    QUuid m_uuid;
    QSize m_modeSize;
    QSize m_physicalSize;
    QPoint m_position;
    qreal m_scale = 1;
    Capabilities m_capabilities;
    Transform m_transform = Transform::Normal;
    QByteArray m_edid;
    QVector<Mode> m_modes;
    DpmsMode m_dpmsMode = DpmsMode::On;
    SubPixel m_subPixel = SubPixel::Unknown;
    int m_refreshRate = -1;
    bool m_isEnabled = true;
    bool m_internal = false;
    bool m_isPlaceholder = false;
    uint32_t m_overscan = 0;
    RgbRange m_rgbRange = RgbRange::Automatic;
    friend class EffectScreenImpl; // to access m_effectScreen
};

inline QRect AbstractOutput::rect() const
{
    return QRect(QPoint(0, 0), geometry().size());
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const AbstractOutput *output);

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::AbstractOutput::Capabilities)

#endif
