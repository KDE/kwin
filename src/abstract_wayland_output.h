/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_WAYLAND_OUTPUT_H
#define KWIN_ABSTRACT_WAYLAND_OUTPUT_H

#include "abstract_output.h"
#include "utils.h"
#include <kwin_export.h>

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QTimer>

namespace KWaylandServer
{
class OutputChangeSet;
}

namespace KWin
{

/**
 * Generic output representation in a Wayland session
 */
class KWIN_EXPORT AbstractWaylandOutput : public AbstractOutput
{
    Q_OBJECT
public:
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

    enum class ModeFlag : uint {
        Current = 0x1,
        Preferred = 0x2,
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    struct Mode
    {
        QSize size;
        int refreshRate;
        ModeFlags flags;
        int id;
    };

    enum class DpmsMode {
        On,
        Standby,
        Suspend,
        Off,
    };

    enum class Capability : uint {
        Dpms = 0x1,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    explicit AbstractWaylandOutput(QObject *parent = nullptr);

    QString name() const override;
    QString uuid() const override;

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
    Transform transform() const;

    int refreshRate() const override;

    bool isInternal() const override {
        return m_internal;
    }

    QString manufacturer() const override;
    QString model() const override;
    QString serialNumber() const override;

    void setGlobalPos(const QPoint &pos);
    void setScale(qreal scale);

    void applyChanges(const KWaylandServer::OutputChangeSet *changeSet) override;

    bool isEnabled() const override;
    void setEnabled(bool enable) override;

    QString description() const;
    Capabilities capabilities() const;
    QByteArray edid() const;
    QVector<Mode> modes() const;
    DpmsMode dpmsMode() const;
    virtual void setDpmsMode(DpmsMode mode);

    /**
     * Returns a matrix that can translate into the display's coordinates system
     */
    static QMatrix4x4 logicalToNativeMatrix(const QRect &rect, qreal scale, Transform transform);

    void recordingStarted();
    void recordingStopped();

    bool isBeingRecorded();

Q_SIGNALS:
    void modeChanged();
    void outputChange(const QRegion &damagedRegion);
    void scaleChanged();
    void transformChanged();
    void dpmsModeChanged();

protected:
    void initialize(const QString &model, const QString &manufacturer,
                    const QString &uuid, const QSize &physicalSize,
                    const QVector<Mode> &modes, const QByteArray &edid);

    QPoint globalPos() const;

    bool internal() const {
        return m_internal;
    }
    void setName(const QString &name) {
        m_name = name;
    }
    void setInternal(bool set) {
        m_internal = set;
    }

    virtual void updateEnablement(bool enable) {
        Q_UNUSED(enable);
    }
    virtual void updateMode(int modeIndex) {
        Q_UNUSED(modeIndex);
    }
    virtual void updateTransform(Transform transform) {
        Q_UNUSED(transform);
    }

    void setCurrentModeInternal(const QSize &size, int refreshRate);
    void setTransformInternal(Transform transform);
    void setDpmsModeInternal(DpmsMode dpmsMode);
    void setCapabilityInternal(Capability capability, bool on = true);

    QSize orientateSize(const QSize &size) const;

private:
    QString m_name;
    QString m_manufacturer;
    QString m_model;
    QString m_serialNumber;
    QString m_uuid;
    QSize m_modeSize;
    QSize m_physicalSize;
    QPoint m_position;
    qreal m_scale = 1;
    Capabilities m_capabilities;
    Transform m_transform = Transform::Normal;
    QByteArray m_edid;
    QVector<Mode> m_modes;
    DpmsMode m_dpmsMode = DpmsMode::On;
    int m_refreshRate = -1;
    int m_recorders = 0;
    bool m_isEnabled = true;
    bool m_internal = false;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::AbstractWaylandOutput::Capabilities)

#endif // KWIN_OUTPUT_H
