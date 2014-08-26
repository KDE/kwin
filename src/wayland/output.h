/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_WAYLAND_OUTPUT_H
#define KWIN_WAYLAND_OUTPUT_H

#include <QObject>
#include <QPoint>
#include <QSize>

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class Output : public QObject
{
    Q_OBJECT
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
    explicit Output(QObject *parent = nullptr);
    virtual ~Output();

    void setup(wl_output *output);

    bool isValid() const;
    operator wl_output*() {
        return m_output;
    }
    operator wl_output*() const {
        return m_output;
    }
    wl_output *output();
    const QSize &physicalSize() const;
    const QPoint &globalPosition() const;
    const QString &manufacturer() const;
    const QString &model() const;
    const QSize &pixelSize() const;
    QRect geometry() const;
    int refreshRate() const;
    int scale() const;
    SubPixel subPixel() const;
    Transform transform() const;

    static void geometryCallback(void *data, wl_output *output, int32_t x, int32_t y,
                                 int32_t physicalWidth, int32_t physicalHeight, int32_t subPixel,
                                 const char *make, const char *model, int32_t transform);
    static void modeCallback(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);
    static void doneCallback(void *data, wl_output *output);
    static void scaleCallback(void *data, wl_output *output, int32_t scale);

Q_SIGNALS:
    void changed();

private:
    void setPhysicalSize(const QSize &size);
    void setGlobalPosition(const QPoint &pos);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setPixelSize(const QSize &size);
    void setRefreshRate(int refreshRate);
    void setScale(int scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);
    static struct wl_output_listener s_outputListener;
    wl_output *m_output;
    QSize m_physicalSize;
    QPoint m_globalPosition;
    QString m_manufacturer;
    QString m_model;
    QSize m_pixelSize;
    int m_refreshRate;
    int m_scale;
    SubPixel m_subPixel;
    Transform m_transform;
};

inline
const QPoint &Output::globalPosition() const
{
    return m_globalPosition;
}

inline
const QString &Output::manufacturer() const
{
    return m_manufacturer;
}

inline
const QString &Output::model() const
{
    return m_model;
}

inline
wl_output *Output::output()
{
    return m_output;
}

inline
const QSize &Output::physicalSize() const
{
    return m_physicalSize;
}

inline
const QSize &Output::pixelSize() const
{
    return m_pixelSize;
}

inline
int Output::refreshRate() const
{
    return m_refreshRate;
}

inline
int Output::scale() const
{
    return m_scale;
}

inline
bool Output::isValid() const
{
    return m_output != nullptr;
}

inline
Output::SubPixel Output::subPixel() const
{
    return m_subPixel;
}

inline
Output::Transform Output::transform() const
{
    return m_transform;
}

}
}

Q_DECLARE_METATYPE(KWin::Wayland::Output::SubPixel)
Q_DECLARE_METATYPE(KWin::Wayland::Output::Transform)

#endif
