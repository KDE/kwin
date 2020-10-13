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
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QVector>

#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/outputdevice_interface.h>

namespace KWaylandServer
{
class OutputInterface;
class OutputDeviceInterface;
class OutputChangeSet;
class OutputManagementInterface;
class XdgOutputV1Interface;
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

    explicit AbstractWaylandOutput(QObject *parent = nullptr);
    ~AbstractWaylandOutput() override;

    QString name() const override;
    QByteArray uuid() const override;

    QSize modeSize() const;

    // TODO: The name is ambiguous. Rename this function.
    QSize pixelSize() const override;

    qreal scale() const override;

    /**
     * The geometry of this output in global compositor co-ordinates (i.e scaled)
     */
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

    /**
     * Current refresh rate in 1/ms.
     */
    int refreshRate() const override;

    bool isInternal() const override {
        return m_internal;
    }

    void setGlobalPos(const QPoint &pos);
    void setScale(qreal scale);

    void applyChanges(const KWaylandServer::OutputChangeSet *changeSet) override;

    QPointer<KWaylandServer::OutputInterface> waylandOutput() const {
        return m_waylandOutput;
    }

    bool isEnabled() const;
    /**
     * Enable or disable the output.
     *
     * This differs from updateDpms as it also removes the wl_output.
     * The default is on.
     */
    void setEnabled(bool enable) override;

    QString description() const;

    /**
     * Returns a matrix that can translate into the display's coordinates system
     */
    static QMatrix4x4 logicalToNativeMatrix(const QRect &rect, qreal scale, Transform transform);

Q_SIGNALS:
    void modeChanged();
    void outputChange(const QRegion &damagedRegion);

protected:
    void initInterfaces(const QString &model, const QString &manufacturer,
                        const QByteArray &uuid, const QSize &physicalSize,
                        const QVector<KWaylandServer::OutputDeviceInterface::Mode> &modes);

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
    void setDpmsSupported(bool set) {
        m_waylandOutput->setDpmsSupported(set);
    }

    virtual void updateEnablement(bool enable) {
        Q_UNUSED(enable);
    }
    virtual void updateDpms(KWaylandServer::OutputInterface::DpmsMode mode) {
        Q_UNUSED(mode);
    }
    virtual void updateMode(int modeIndex) {
        Q_UNUSED(modeIndex);
    }
    virtual void updateTransform(Transform transform) {
        Q_UNUSED(transform);
    }

    void setWaylandMode(const QSize &size, int refreshRate);
    void setTransform(Transform transform);

    QSize orientateSize(const QSize &size) const;

private:
    void setTransform(KWaylandServer::OutputDeviceInterface::Transform transform);

    KWaylandServer::OutputInterface *m_waylandOutput;
    KWaylandServer::XdgOutputV1Interface *m_xdgOutputV1;
    KWaylandServer::OutputDeviceInterface *m_waylandOutputDevice;
    KWaylandServer::OutputInterface::DpmsMode m_dpms = KWaylandServer::OutputInterface::DpmsMode::On;

    QString m_name;
    bool m_internal = false;
};

}

#endif // KWIN_OUTPUT_H
