/********************************************************************
Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_OUTPUTDEVICE_INTERFACE_H
#define WAYLAND_SERVER_OUTPUTDEVICE_INTERFACE_H

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QVector>

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"

struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;

/** @class OutputDeviceInterface
 *
 * Represents an output device, the difference to Output is that this output can be disabled,
 * so not currently used to display content.
 *
 * @see OutputManagementInterface
 * @since 5.5
 */
class KWAYLANDSERVER_EXPORT OutputDeviceInterface : public Global
{
    Q_OBJECT
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize NOTIFY physicalSizeChanged)
    Q_PROPERTY(QPoint globalPosition READ globalPosition WRITE setGlobalPosition NOTIFY globalPositionChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber WRITE setSerialNumber NOTIFY serialNumberChanged)
    Q_PROPERTY(QString eisaId READ eisaId WRITE setEisaId NOTIFY eisaIdChanged)
    Q_PROPERTY(QSize pixelSize READ pixelSize NOTIFY pixelSizeChanged)
    Q_PROPERTY(int refreshRate READ refreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(qreal scale READ scaleF WRITE setScaleF NOTIFY scaleFChanged)
    Q_PROPERTY(QByteArray edid READ edid WRITE setEdid NOTIFY edidChanged)
    Q_PROPERTY(OutputDeviceInterface::Enablement enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QByteArray uuid READ uuid WRITE setUuid NOTIFY uuidChanged)
public:
    enum class SubPixel {
        Unknown,
        None,
        HorizontalRGB,
        HorizontalBGR,
        VerticalRGB,
        VerticalBGR
    };
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
    enum class Enablement {
        Disabled = 0,
        Enabled = 1
    };
    enum class ModeFlag {
        Current = 1,
        Preferred = 2
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)
    struct Mode {
        QSize size = QSize();
        int refreshRate = 60000;
        ModeFlags flags;
        int id = -1;
    };
    struct ColorCurves {
        QVector<quint16> red, green, blue;
        bool operator==(const ColorCurves &cc) const;
        bool operator!=(const ColorCurves &cc) const;
    };
    virtual ~OutputDeviceInterface();

    QSize physicalSize() const;
    QPoint globalPosition() const;
    QString manufacturer() const;
    QString model() const;
    QString serialNumber() const;
    QString eisaId() const;
    QSize pixelSize() const;
    int refreshRate() const;
    int scale() const;
    qreal scaleF() const;
    SubPixel subPixel() const;
    Transform transform() const;
    ColorCurves colorCurves() const;
    QList<Mode> modes() const;
    int currentModeId() const;

    QByteArray edid() const;
    OutputDeviceInterface::Enablement enabled() const;
    QByteArray uuid() const;

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setSerialNumber(const QString &serialNumber);
    void setEisaId(const QString &eisaId);
    void setScale(int scale);
    void setScaleF(qreal scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);
    void setColorCurves(const ColorCurves &colorCurves);
    void addMode(Mode &mode);
    void setCurrentMode(const int modeId);

    void setEdid(const QByteArray &edid);
    void setEnabled(OutputDeviceInterface::Enablement enabled);
    void setUuid(const QByteArray &uuid);

    static OutputDeviceInterface *get(wl_resource *native);
    static QList<OutputDeviceInterface *>list();

Q_SIGNALS:
    void physicalSizeChanged(const QSize&);
    void globalPositionChanged(const QPoint&);
    void manufacturerChanged(const QString&);
    void modelChanged(const QString&);
    void serialNumberChanged(const QString&);
    void eisaIdChanged(const QString &);
    void pixelSizeChanged(const QSize&);
    void refreshRateChanged(int);
    //@deprecated see scaleChanged(real)
    void scaleChanged(int);
    void scaleFChanged(qreal);
    void subPixelChanged(SubPixel);
    void transformChanged(Transform);
    void colorCurvesChanged(ColorCurves);
    void modesChanged();
    void currentModeChanged();

    void edidChanged();
    void enabledChanged();
    void uuidChanged();

private:
    friend class Display;
    explicit OutputDeviceInterface(Display *display, QObject *parent = nullptr);
    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWayland::Server::OutputDeviceInterface::ModeFlags)
Q_DECLARE_METATYPE(KWayland::Server::OutputDeviceInterface::Enablement)
Q_DECLARE_METATYPE(KWayland::Server::OutputDeviceInterface::SubPixel)
Q_DECLARE_METATYPE(KWayland::Server::OutputDeviceInterface::Transform)
Q_DECLARE_METATYPE(KWayland::Server::OutputDeviceInterface::ColorCurves)

#endif
