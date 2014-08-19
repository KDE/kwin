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
#include "output.h"

#include <QRect>

namespace KWin
{

namespace Wayland
{

Output::Output(QObject *parent)
    : QObject(parent)
    , m_output(nullptr)
    , m_physicalSize()
    , m_globalPosition()
    , m_manufacturer()
    , m_model()
    , m_pixelSize()
    , m_refreshRate(0)
    , m_scale(1)
    , m_subPixel(SubPixel::Unknown)
    , m_transform(Transform::Normal)
{
}

Output::~Output()
{
    if (m_output) {
        wl_output_destroy(m_output);
    }
}

wl_output_listener Output::s_outputListener = {
    Output::geometryCallback,
    Output::modeCallback,
    Output::doneCallback,
    Output::scaleCallback
};

void Output::geometryCallback(void *data, wl_output *output,
                              int32_t x, int32_t y,
                              int32_t physicalWidth, int32_t physicalHeight,
                              int32_t subPixel, const char *make, const char *model, int32_t transform)
{
    Q_UNUSED(transform)
    Output *o = reinterpret_cast<Output*>(data);
    Q_ASSERT(o->output() == output);
    o->setGlobalPosition(QPoint(x, y));
    o->setManufacturer(make);
    o->setModel(model);
    o->setPhysicalSize(QSize(physicalWidth, physicalHeight));
    auto toSubPixel = [subPixel]() {
        switch (subPixel) {
        case WL_OUTPUT_SUBPIXEL_NONE:
            return SubPixel::None;
        case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
            return SubPixel::HorizontalRGB;
        case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
            return SubPixel::HorizontalBGR;
        case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
            return SubPixel::VerticalRGB;
        case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
            return SubPixel::VerticalBGR;
        case WL_OUTPUT_SUBPIXEL_UNKNOWN:
        default:
            return SubPixel::Unknown;
        }
    };
    o->setSubPixel(toSubPixel());
    auto toTransform = [transform]() {
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
            return Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return Transform::Normal;
        }
    };
    o->setTransform(toTransform());
}

void Output::modeCallback(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
    if (!(flags & WL_OUTPUT_MODE_CURRENT)) {
        // ignore all non-current modes;
        return;
    }
    Output *o = reinterpret_cast<Output*>(data);
    Q_ASSERT(o->output() == output);
    o->setPixelSize(QSize(width, height));
    o->setRefreshRate(refresh);
}

void Output::scaleCallback(void *data, wl_output *output, int32_t scale)
{
    Output *o = reinterpret_cast<Output*>(data);
    Q_ASSERT(o->output() == output);
    o->setScale(scale);
}

void Output::doneCallback(void *data, wl_output *output)
{
    Output *o = reinterpret_cast<Output*>(data);
    Q_ASSERT(o->output() == output);
    o->changed();
}

void Output::setup(wl_output *output)
{
    Q_ASSERT(output);
    Q_ASSERT(!m_output);
    m_output = output;
    wl_output_add_listener(m_output, &s_outputListener, this);
}

void Output::setGlobalPosition(const QPoint &pos)
{
    m_globalPosition = pos;
}

void Output::setManufacturer(const QString &manufacturer)
{
    m_manufacturer = manufacturer;
}

void Output::setModel(const QString &model)
{
    m_model = model;
}

void Output::setPhysicalSize(const QSize &size)
{
    m_physicalSize = size;
}

void Output::setPixelSize(const QSize& size)
{
    m_pixelSize = size;
}

void Output::setRefreshRate(int refreshRate)
{
    m_refreshRate = refreshRate;
}

void Output::setScale(int scale)
{
    m_scale = scale;
}

QRect Output::geometry() const
{
    if (!m_pixelSize.isValid()) {
        return QRect();
    }
    return QRect(m_globalPosition, m_pixelSize);
}

void Output::setSubPixel(Output::SubPixel subPixel)
{
    m_subPixel = subPixel;
}

void Output::setTransform(Output::Transform transform)
{
    m_transform = transform;
}

}
}
