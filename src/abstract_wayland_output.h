/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_WAYLAND_OUTPUT_H
#define KWIN_ABSTRACT_WAYLAND_OUTPUT_H

#include "abstract_output.h"
#include "utils/common.h"
#include "renderloop.h"
#include <kwin_export.h>

#include <QObject>
#include <QTimer>
#include <QSize>

namespace KWin
{

class WaylandOutputConfig;

/**
 * Generic output representation in a Wayland session
 */
class KWIN_EXPORT AbstractWaylandOutput : public AbstractOutput
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

    explicit AbstractWaylandOutput(QObject *parent = nullptr);

    QString name() const override;
    QUuid uuid() const override;

    QSize modeSize() const;

    // TODO: The name is ambiguous. Rename this function.
    QSize pixelSize() const override;
    qreal scale() const override;
    QRect geometry() const override;
    QSize physicalSize() const override;

    /**
     * Returns the orientation of this output.
     *
     * - Flipped along the vertical axis is landscape + inv. portrait.
     * - Rotated 90° and flipped along the horizontal axis is portrait + inv. landscape
     * - Rotated 180° and flipped along the vertical axis is inv. landscape + inv. portrait
     * - Rotated 270° and flipped along the horizontal axis is inv. portrait + inv. landscape +
     *   portrait
     */
    Transform transform() const override;

    int refreshRate() const override;

    bool isInternal() const override {
        return m_internal;
    }

    QString eisaId() const;
    QString manufacturer() const override;
    QString model() const override;
    QString serialNumber() const override;

    void moveTo(const QPoint &pos);
    void setScale(qreal scale);

    void applyChanges(const WaylandOutputConfig &config);

    bool isEnabled() const override;
    void setEnabled(bool enable) override;

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

    void recordingStarted();
    void recordingStopped();

    bool isBeingRecorded();

    void setVrrPolicy(RenderLoop::VrrPolicy policy);
    RenderLoop::VrrPolicy vrrPolicy() const;
    RgbRange rgbRange() const;

    bool isPlaceholder() const;

Q_SIGNALS:
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

    void setName(const QString &name) {
        m_name = name;
    }
    void setInternal(bool set) {
        m_internal = set;
    }

    virtual void updateEnablement(bool enable) {
        Q_UNUSED(enable);
    }
    virtual void updateTransform(Transform transform) {
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

    QSize orientateSize(const QSize &size) const;

private:
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
    int m_recorders = 0;
    bool m_isEnabled = true;
    bool m_internal = false;
    bool m_isPlaceholder = false;
    uint32_t m_overscan = 0;
    RgbRange m_rgbRange = RgbRange::Automatic;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::AbstractWaylandOutput::Capabilities)

#endif // KWIN_OUTPUT_H
