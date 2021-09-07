/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QUuid>
#include <QVector>

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
class KWAYLANDSERVER_EXPORT OutputDeviceV2Interface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize NOTIFY physicalSizeChanged)
    Q_PROPERTY(QPoint globalPosition READ globalPosition WRITE setGlobalPosition NOTIFY globalPositionChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber WRITE setSerialNumber NOTIFY serialNumberChanged)
    Q_PROPERTY(QString eisaId READ eisaId WRITE setEisaId NOTIFY eisaIdChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QByteArray edid READ edid WRITE setEdid NOTIFY edidChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QUuid uuid READ uuid WRITE setUuid NOTIFY uuidChanged)
    Q_PROPERTY(Capabilities capabilities READ capabilities WRITE setCapabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(uint32_t overscan READ overscan WRITE setOverscan NOTIFY overscanChanged)
    Q_PROPERTY(VrrPolicy vrrPolicy READ vrrPolicy WRITE setVrrPolicy NOTIFY vrrPolicyChanged)
    Q_PROPERTY(RgbRange rgbRange READ rgbRange WRITE setRgbRange NOTIFY rgbRangeChanged)
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

    static OutputDeviceV2Interface *get(wl_resource *native);

Q_SIGNALS:
    void physicalSizeChanged(const QSize&);
    void globalPositionChanged(const QPoint&);
    void manufacturerChanged(const QString&);
    void modelChanged(const QString&);
    void serialNumberChanged(const QString&);
    void eisaIdChanged(const QString &);
    void scaleChanged(qreal);
    void subPixelChanged(SubPixel);
    void transformChanged(Transform);
    void modesChanged();
    void currentModeChanged();

    void edidChanged();
    void enabledChanged();
    void uuidChanged();

    void capabilitiesChanged();
    void overscanChanged();
    void vrrPolicyChanged();
    void rgbRangeChanged();

private:
    QScopedPointer<OutputDeviceV2InterfacePrivate> d;
};

/**
 * @class OutputDeviceModeV2Interface
 *
 * Represents an output device mode.
 *
* @see OutputDeviceV2Interface
 */
class KWAYLANDSERVER_EXPORT OutputDeviceModeV2Interface : public QObject
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
    QScopedPointer<OutputDeviceModeV2InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceModeV2Interface::ModeFlag)
Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceV2Interface::SubPixel)
Q_DECLARE_METATYPE(KWaylandServer::OutputDeviceV2Interface::Transform)
