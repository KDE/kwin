/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QUuid>
#include <QVector>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class OutputDeviceV2InterfacePrivate;
class OutputDeviceModeV2Interface;
class OutputDeviceModeV2InterfacePrivate;

/** @class OutputDeviceV2Interface
 *
 * Represents an output device, the difference to Output is that this output can be disabled,
 * so not currently used to display content.
 *
 * @see OutputManagementV2Interface
 */
class KWIN_EXPORT OutputDeviceV2Interface : public QObject
{
    Q_OBJECT
public:
    enum class SubPixel {
        Unknown,
        None,
        HorizontalRGB,
        HorizontalBGR,
        VerticalRGB,
        VerticalBGR,
    };
    Q_ENUM(SubPixel)
    enum class Transform {
        Normal,
        Rotated90,
        Rotated180,
        Rotated270,
        Flipped,
        Flipped90,
        Flipped180,
        Flipped270,
    };
    Q_ENUM(Transform)
    enum class Capability {
        Overscan = 0x1,
        Vrr = 0x2,
        RgbRange = 0x4,
    };
    Q_ENUM(Capability)
    Q_DECLARE_FLAGS(Capabilities, Capability)
    enum class VrrPolicy {
        Never = 0,
        Always = 1,
        Automatic = 2
    };
    Q_ENUM(VrrPolicy)
    enum class RgbRange {
        Automatic = 0,
        Full = 1,
        Limited = 2,
    };
    Q_ENUM(RgbRange)

    explicit OutputDeviceV2Interface(Display *display, QObject *parent = nullptr);
    ~OutputDeviceV2Interface() override;

    void remove();

    QSize physicalSize() const;
    QPoint globalPosition() const;
    QString manufacturer() const;
    QString model() const;
    QString serialNumber() const;
    QString eisaId() const;
    QString name() const;
    QSize pixelSize() const;
    int refreshRate() const;

    qreal scale() const;
    SubPixel subPixel() const;
    Transform transform() const;

    QByteArray edid() const;
    bool enabled() const;
    QUuid uuid() const;

    Capabilities capabilities() const;
    uint32_t overscan() const;
    VrrPolicy vrrPolicy() const;
    RgbRange rgbRange() const;

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setSerialNumber(const QString &serialNumber);
    void setEisaId(const QString &eisaId);
    void setName(const QString &name);

    void setScale(qreal scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);

    void setModes(const QList<KWaylandServer::OutputDeviceModeV2Interface *> &modes);
    void setCurrentMode(KWaylandServer::OutputDeviceModeV2Interface *mode);

    /**
     * Makes the mode with the specified @a size and @a refreshRate current.
     * Returns @c false if no mode with the given attributes exists; otherwise returns @c true.
     */
    bool setCurrentMode(const QSize &size, int refreshRate);

    void setEdid(const QByteArray &edid);
    void setEnabled(bool enabled);
    void setUuid(const QUuid &uuid);

    void setCapabilities(Capabilities cap);
    void setOverscan(uint32_t overscan);
    void setVrrPolicy(VrrPolicy policy);
    void setRgbRange(RgbRange rgbRange);

    wl_resource *resource() const;
    static OutputDeviceV2Interface *get(wl_resource *native);

private:
    std::unique_ptr<OutputDeviceV2InterfacePrivate> d;
};

/**
 * @class OutputDeviceModeV2Interface
 *
 * Represents an output device mode.
 *
 * @see OutputDeviceV2Interface
 */
class KWIN_EXPORT OutputDeviceModeV2Interface : public QObject
{
    Q_OBJECT
public:
    enum class ModeFlag {
        Current = 0x1,
        Preferred = 0x2,
    };
    Q_ENUM(ModeFlag)
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    OutputDeviceModeV2Interface(const QSize &size, int refreshRate, ModeFlags flags, QObject *parent = nullptr);
    ~OutputDeviceModeV2Interface() override;

    QSize size() const;
    int refreshRate() const;
    OutputDeviceModeV2Interface::ModeFlags flags() const;

    void setFlags(OutputDeviceModeV2Interface::ModeFlags newFlags);

    static OutputDeviceModeV2Interface *get(wl_resource *native);

private:
    friend class OutputDeviceModeV2InterfacePrivate;
    std::unique_ptr<OutputDeviceModeV2InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceModeV2Interface::ModeFlag)
Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceV2Interface::SubPixel)
Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceV2Interface::Transform)
