/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_SERVER_OUTPUT_INTERFACE_H
#define KWIN_WAYLAND_SERVER_OUTPUT_INTERFACE_H

#include <QObject>
#include <QPoint>
#include <QSize>

#include <kwaylandserver_export.h>

struct wl_global;
struct wl_client;
struct wl_resource;

namespace KWin
{
namespace WaylandServer
{

class Display;

class KWAYLANDSERVER_EXPORT OutputInterface : public QObject
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
    virtual ~OutputInterface();
    void create();
    void destroy();
    bool isValid() const {
        return m_output != nullptr;
    }

    const QSize &physicalSize() const {
        return m_physicalSize;
    }
    const QPoint &globalPosition() const {
        return m_globalPosition;
    }
    const QString &manufacturer() const {
        return m_manufacturer;
    }
    const QString &model() const {
        return m_model;
    }
    QSize pixelSize() const;
    int refreshRate() const;
    int scale() const {
        return m_scale;
    }
    SubPixel subPixel() const {
        return m_subPixel;
    }
    Transform transform() const {
        return m_transform;
    }
    QList<Mode> modes() const {
        return m_modes;
    }

    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setScale(int scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);
    void addMode(const QSize &size, ModeFlags flags = ModeFlags(), int refreshRate = 60000);
    void setCurrentMode(const QSize &size, int refreshRate = 60000);

    operator wl_global*() {
        return m_output;
    }
    operator wl_global*() const {
        return m_output;
    }

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

private:
    struct ResourceData {
        wl_resource *resource;
        uint32_t version;
    };
    friend class Display;
    explicit OutputInterface(Display *display, QObject *parent = nullptr);
    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void unbind(wl_resource *resource);
    void bind(wl_client *client, uint32_t version, uint32_t id);
    int32_t toTransform() const;
    int32_t toSubPixel() const;
    void sendMode(wl_resource *resource, const Mode &mode);
    void sendDone(const ResourceData &data);
    void updateGeometry();
    void sendGeometry(wl_resource *resource);
    void sendScale(const ResourceData &data);
    void updateScale();
    Display *m_display;
    wl_global *m_output;
    QSize m_physicalSize;
    QPoint m_globalPosition;
    QString m_manufacturer;
    QString m_model;
    int m_scale;
    SubPixel m_subPixel;
    Transform m_transform;
    QList<Mode> m_modes;
    QList<ResourceData> m_resources;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::WaylandServer::OutputInterface::ModeFlags)
Q_DECLARE_METATYPE(KWin::WaylandServer::OutputInterface::SubPixel)
Q_DECLARE_METATYPE(KWin::WaylandServer::OutputInterface::Transform)

#endif
