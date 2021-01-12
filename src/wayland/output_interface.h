/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_OUTPUT_INTERFACE_H
#define WAYLAND_SERVER_OUTPUT_INTERFACE_H

#include <QObject>
#include <QPoint>
#include <QSize>

#include <KWaylandServer/kwaylandserver_export.h>
#include "global.h"

struct wl_global;
struct wl_client;
struct wl_resource;

namespace KWaylandServer
{

class ClientConnection;
class Display;

/**
 * @brief Global for the wl_output interface.
 *
 **/
class KWAYLANDSERVER_EXPORT OutputInterface : public Global
{
    Q_OBJECT
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize NOTIFY physicalSizeChanged)
    Q_PROPERTY(QPoint globalPosition READ globalPosition WRITE setGlobalPosition NOTIFY globalPositionChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QSize pixelSize READ pixelSize NOTIFY pixelSizeChanged)
    Q_PROPERTY(int refreshRate READ refreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(int scale READ scale WRITE setScale NOTIFY scaleChanged)
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
    enum class ModeFlag {
        Current = 1,
        Preferred = 2
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)
    struct Mode {
        QSize size = QSize();
        int refreshRate = 60000;
        ModeFlags flags;
    };
    enum class DpmsMode {
        On,
        Standby,
        Suspend,
        Off
    };

    explicit OutputInterface(Display *display, QObject *parent = nullptr);
    virtual ~OutputInterface();

    QSize physicalSize() const;
    QPoint globalPosition() const;
    QString manufacturer() const;
    QString model() const;
    QSize pixelSize() const;
    int refreshRate() const;
    int scale() const;
    SubPixel subPixel() const;
    Transform transform() const;
    QList<Mode> modes() const;
    bool isDpmsSupported() const;
    DpmsMode dpmsMode() const;

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setScale(int scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);
    void addMode(const QSize &size, ModeFlags flags = ModeFlags(), int refreshRate = 60000);
    void setCurrentMode(const QSize &size, int refreshRate = 60000);

    /**
     * Sets whether Dpms is supported for this output.
     * Default is @c false.
     * @since 5.5
     **/
    void setDpmsSupported(bool supported);
    /**
     * Sets the currently used dpms mode.
     * Default is @c DpmsMode::On.
     * @since 5.5
     **/
    void setDpmsMode(DpmsMode mode);

    /**
     * @returns all wl_resources bound for the @p client
     * @since 5.27
     **/
    QVector<wl_resource *> clientResources(ClientConnection *client) const;

    /**
     * Returns @c true if the output is on; otherwise returns false.
     * @since 5.20
     */
    bool isEnabled() const;

    static OutputInterface *get(wl_resource *native);

Q_SIGNALS:
    void physicalSizeChanged(const QSize&);
    void globalPositionChanged(const QPoint&);
    void manufacturerChanged(const QString&);
    void modelChanged(const QString&);
    void pixelSizeChanged(const QSize&);
    void refreshRateChanged(int);
    void scaleChanged(int);
    void subPixelChanged(SubPixel);
    void transformChanged(Transform);
    void modesChanged();
    void currentModeChanged();
    void dpmsModeChanged();
    void dpmsSupportedChanged();

    /**
     * Change of dpms @p mode is requested.
     * A server is free to ignore this request.
     * @since 5.5
     **/
    void dpmsModeRequested(KWaylandServer::OutputInterface::DpmsMode mode);

    /**
     * Emitted when a client binds to a given output
     * @internal
     */
    void bound(ClientConnection *client,  wl_resource *boundResource);

private:
    class Private;
    Private *d_func() const;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWaylandServer::OutputInterface::ModeFlags)
Q_DECLARE_METATYPE(KWaylandServer::OutputInterface::SubPixel)
Q_DECLARE_METATYPE(KWaylandServer::OutputInterface::Transform)
Q_DECLARE_METATYPE(KWaylandServer::OutputInterface::DpmsMode)

#endif
