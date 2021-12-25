/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_OUTPUT_H
#define KWIN_ABSTRACT_OUTPUT_H

#include <kwin_export.h>

#include <QDebug>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QUuid>
#include <QVector>

namespace KWaylandServer
{
class OutputChangeSetV2;
}

namespace KWin
{
class EffectScreenImpl;
class RenderLoop;

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
    explicit AbstractOutput(QObject *parent = nullptr);
    ~AbstractOutput() override;

    /**
     * Returns a short identifiable name of this output.
     */
    virtual QString name() const = 0;

    /**
     * Returns the identifying uuid of this output.
     *
     * Default implementation returns an empty byte array.
     */
    virtual QUuid uuid() const;

    /**
     * Returns @c true if the output is enabled; otherwise returns @c false.
     */
    virtual bool isEnabled() const;

    /**
     * Enable or disable the output.
     *
     * Default implementation does nothing
     */
    virtual void setEnabled(bool enable);

    /**
     * Returns geometry of this output in device independent pixels.
     */
    virtual QRect geometry() const = 0;

    /**
     * Returns the approximate vertical refresh rate of this output, in mHz.
     */
    virtual int refreshRate() const = 0;

    /**
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     *
     * Default implementation returns @c false.
     */
    virtual bool isInternal() const;

    /**
     * Returns the ratio between physical pixels and logical pixels.
     *
     * Default implementation returns 1.
     */
    virtual qreal scale() const;

    /**
     * Returns the physical size of this output, in millimeters.
     *
     * Default implementation returns an invalid QSize.
     */
    virtual QSize physicalSize() const;

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
    virtual QSize pixelSize() const = 0;

    /**
     * Returns the manufacturer of the screen.
     */
    virtual QString manufacturer() const;
    /**
     * Returns the model of the screen.
     */
    virtual QString model() const;
    /**
     * Returns the serial number of the screen.
     */
    virtual QString serialNumber() const;

    /**
     * Returns the RenderLoop for this output. This function returns @c null if the
     * underlying platform doesn't support per-screen rendering mode.
     */
    virtual RenderLoop *renderLoop() const;

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
    virtual Transform transform() const { return Transform::Normal; }

    virtual bool usesSoftwareCursor() const;

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

private:
    Q_DISABLE_COPY(AbstractOutput)
    EffectScreenImpl *m_effectScreen = nullptr;
    int m_directScanoutCount = 0;
    friend class EffectScreenImpl; // to access m_effectScreen
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const AbstractOutput *output);

} // namespace KWin

#endif
