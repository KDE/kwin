/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/output.h"
#include "kwin_export.h"

#include <QObject>
#include <QPoint>
#include <QSize>

struct wl_resource;
struct wl_client;

namespace KWin
{
class Output;
}

namespace KWaylandServer
{
class ClientConnection;
class Display;
class OutputInterfacePrivate;

/**
 * The OutputInterface class represents a screen. This class corresponds to the Wayland
 * interface @c wl_output.
 */
class KWIN_EXPORT OutputInterface : public QObject
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
    struct Mode
    {
        QSize size = QSize();
        int refreshRate = 60000;
    };

    explicit OutputInterface(Display *display, KWin::Output *handle);
    ~OutputInterface() override;

    bool isRemoved() const;
    void remove();

    KWin::Output *handle() const;

    QSize physicalSize() const;
    QPoint globalPosition() const;
    QString manufacturer() const;
    QString model() const;
    QSize pixelSize() const;
    int refreshRate() const;
    int scale() const;
    KWin::Output::SubPixel subPixel() const;
    KWin::Output::Transform transform() const;
    Mode mode() const;

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setScale(int scale);
    void setSubPixel(KWin::Output::SubPixel subPixel);
    void setTransform(KWin::Output::Transform transform);
    void setMode(const Mode &mode);
    void setMode(const QSize &size, int refreshRate = 60000);
    void setName(const QString &name);
    void setDescription(const QString &description);

    /**
     * @returns all wl_resources bound for the @p client
     */
    QVector<wl_resource *> clientResources(ClientConnection *client) const;

    /**
     * Submit changes to all clients.
     */
    void done();

    /**
     * Submit changes to @p client.
     */
    void done(wl_client *client);

    static OutputInterface *get(wl_resource *native);

    Display *display() const;

Q_SIGNALS:
    void physicalSizeChanged(const QSize &);
    void globalPositionChanged(const QPoint &);
    void manufacturerChanged(const QString &);
    void modelChanged(const QString &);
    void pixelSizeChanged(const QSize &);
    void refreshRateChanged(int);
    void scaleChanged(int);
    void subPixelChanged(KWin::Output::SubPixel);
    void transformChanged(KWin::Output::Transform);
    void modeChanged();
    void removed();

    /**
     * Emitted when a client binds to a given output
     * @internal
     */
    void bound(ClientConnection *client, wl_resource *boundResource);

private:
    std::unique_ptr<OutputInterfacePrivate> d;
};

} // namespace KWaylandServer
