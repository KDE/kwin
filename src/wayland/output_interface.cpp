/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "output_interface.h"
#include "display.h"

#include <wayland-server.h>

namespace KWin
{
namespace WaylandServer
{

static const quint32 s_version = 2;

OutputInterface::OutputInterface(Display *display, QObject *parent)
    : QObject(parent)
    , m_display(display)
    , m_output(nullptr)
    , m_physicalSize(QSize())
    , m_globalPosition(QPoint())
    , m_manufacturer(QStringLiteral("org.kde.kwin"))
    , m_model(QStringLiteral("none"))
    , m_scale(1)
    , m_subPixel(SubPixel::Unknown)
    , m_transform(Transform::Normal)
{
    connect(this, &OutputInterface::currentModeChanged, this,
        [this] {
            auto currentModeIt = std::find_if(m_modes.constBegin(), m_modes.constEnd(), [](const Mode &mode) { return mode.flags.testFlag(ModeFlag::Current); });
            if (currentModeIt == m_modes.constEnd()) {
                return;
            }
            for (auto it = m_resources.constBegin(); it != m_resources.constEnd(); ++it) {
                sendMode((*it).resource, *currentModeIt);
                sendDone(*it);
            }
        }
    );
    connect(this, &OutputInterface::subPixelChanged,       this, &OutputInterface::updateGeometry);
    connect(this, &OutputInterface::transformChanged,      this, &OutputInterface::updateGeometry);
    connect(this, &OutputInterface::globalPositionChanged, this, &OutputInterface::updateGeometry);
    connect(this, &OutputInterface::modelChanged,          this, &OutputInterface::updateGeometry);
    connect(this, &OutputInterface::manufacturerChanged,   this, &OutputInterface::updateGeometry);
    connect(this, &OutputInterface::scaleChanged,          this, &OutputInterface::updateScale);
}

OutputInterface::~OutputInterface()
{
    destroy();
}

void OutputInterface::create()
{
    Q_ASSERT(!m_output);
    m_output = wl_global_create(*m_display, &wl_output_interface, s_version, this, OutputInterface::bind);
}

void OutputInterface::destroy()
{
    if (!m_output) {
        return;
    }
    wl_global_destroy(m_output);
    m_output = nullptr;
}

QSize OutputInterface::pixelSize() const
{
    auto it = std::find_if(m_modes.begin(), m_modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (it == m_modes.end()) {
        return QSize();
    }
    return (*it).size;
}

int OutputInterface::refreshRate() const
{
    auto it = std::find_if(m_modes.begin(), m_modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (it == m_modes.end()) {
        return 60000;
    }
    return (*it).refreshRate;
}

void OutputInterface::addMode(const QSize &size, OutputInterface::ModeFlags flags, int refreshRate)
{
    Q_ASSERT(!isValid());

    auto currentModeIt = std::find_if(m_modes.begin(), m_modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt == m_modes.end() && !flags.testFlag(ModeFlag::Current)) {
        // no mode with current flag - enforce
        flags |= ModeFlag::Current;
    }
    if (currentModeIt != m_modes.end() && flags.testFlag(ModeFlag::Current)) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    if (flags.testFlag(ModeFlag::Preferred)) {
        // remove from existing Preferred mode
        auto preferredIt = std::find_if(m_modes.begin(), m_modes.end(),
            [](const Mode &mode) {
                return mode.flags.testFlag(ModeFlag::Preferred);
            }
        );
        if (preferredIt != m_modes.end()) {
            (*preferredIt).flags &= ~uint(ModeFlag::Preferred);
        }
    }

    auto existingModeIt = std::find_if(m_modes.begin(), m_modes.end(),
        [size,refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );
    auto emitChanges = [this,flags,size,refreshRate] {
        emit modesChanged();
        if (flags.testFlag(ModeFlag::Current)) {
            emit refreshRateChanged(refreshRate);
            emit pixelSizeChanged(size);
            emit currentModeChanged();
        }
    };
    if (existingModeIt != m_modes.end()) {
        if ((*existingModeIt).flags == flags) {
            // nothing to do
            return;
        }
        (*existingModeIt).flags = flags;
        emitChanges();
        return;
    }
    Mode mode;
    mode.size = size;
    mode.refreshRate = refreshRate;
    mode.flags = flags;
    m_modes << mode;
    emitChanges();
}

void OutputInterface::setCurrentMode(const QSize &size, int refreshRate)
{
    auto currentModeIt = std::find_if(m_modes.begin(), m_modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt != m_modes.end()) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    auto existingModeIt = std::find_if(m_modes.begin(), m_modes.end(),
        [size,refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );

    Q_ASSERT(existingModeIt != m_modes.end());
    (*existingModeIt).flags |= ModeFlag::Current;
    emit modesChanged();
    emit refreshRateChanged((*existingModeIt).refreshRate);
    emit pixelSizeChanged((*existingModeIt).size);
    emit currentModeChanged();
}

void OutputInterface::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    OutputInterface *output = reinterpret_cast<OutputInterface*>(data);
    output->bind(client, version, id);
}

int32_t OutputInterface::toTransform() const
{
    switch (m_transform) {
    case Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

int32_t OutputInterface::toSubPixel() const
{
    switch (m_subPixel) {
    case SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

void OutputInterface::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_output_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_user_data(resource, this);
    wl_resource_set_destructor(resource, OutputInterface::unbind);
    ResourceData r;
    r.resource = resource;
    r.version = version;
    m_resources << r;

    sendGeometry(resource);
    sendScale(r);

    auto currentModeIt = m_modes.constEnd();
    for (auto it = m_modes.constBegin(); it != m_modes.constEnd(); ++it) {
        const Mode &mode = *it;
        if (mode.flags.testFlag(ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(resource, mode);
    }

    if (currentModeIt != m_modes.constEnd()) {
        sendMode(resource, *currentModeIt);
    }

    sendDone(r);
}

void OutputInterface::unbind(wl_resource *resource)
{
    OutputInterface *o = reinterpret_cast<OutputInterface*>(wl_resource_get_user_data(resource));
    auto it = std::find_if(o->m_resources.begin(), o->m_resources.end(), [resource](const ResourceData &r) { return r.resource == resource; });
    if (it != o->m_resources.end()) {
        o->m_resources.erase(it);
    }
}

void OutputInterface::sendMode(wl_resource *resource, const Mode &mode)
{
    int32_t flags = 0;
    if (mode.flags.testFlag(ModeFlag::Current)) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    wl_output_send_mode(resource,
                        flags,
                        mode.size.width(),
                        mode.size.height(),
                        mode.refreshRate);

}

void OutputInterface::sendGeometry(wl_resource *resource)
{
    wl_output_send_geometry(resource,
                            m_globalPosition.x(),
                            m_globalPosition.y(),
                            m_physicalSize.width(),
                            m_physicalSize.height(),
                            toSubPixel(),
                            qPrintable(m_manufacturer),
                            qPrintable(m_model),
                            toTransform());
}

void OutputInterface::sendScale(const OutputInterface::ResourceData &data)
{
    if (data.version < 2) {
        return;
    }
    wl_output_send_scale(data.resource, m_scale);
}

void OutputInterface::sendDone(const OutputInterface::ResourceData &data)
{
    if (data.version < 2) {
        return;
    }
    wl_output_send_done(data.resource);
}

void OutputInterface::updateGeometry()
{
    for (auto it = m_resources.constBegin(); it != m_resources.constEnd(); ++it) {
        sendGeometry((*it).resource);
        sendDone(*it);
    }
}

void OutputInterface::updateScale()
{
    for (auto it = m_resources.constBegin(); it != m_resources.constEnd(); ++it) {
        sendScale(*it);
        sendDone(*it);
    }
}

#define SETTER(setterName, type, argumentName) \
    void OutputInterface::setterName(type arg) \
    { \
        if (m_##argumentName == arg) { \
            return; \
        } \
        m_##argumentName = arg; \
        emit argumentName##Changed(m_##argumentName); \
    }

SETTER(setPhysicalSize, const QSize&, physicalSize)
SETTER(setGlobalPosition, const QPoint&, globalPosition)
SETTER(setManufacturer, const QString&, manufacturer)
SETTER(setModel, const QString&, model)
SETTER(setScale, int, scale)
SETTER(setSubPixel, SubPixel, subPixel)
SETTER(setTransform, Transform, transform)

#undef SETTER

}
}
