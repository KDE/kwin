/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

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
#include "abstract_output.h"
#include "wayland_server.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/outputchangeset.h>
#include <KWayland/Server/xdgoutput_interface.h>
// KF5
#include <KLocalizedString>

#include <cmath>

namespace KWin
{

AbstractOutput::AbstractOutput(QObject *parent)
    : QObject(parent)
{
}

AbstractOutput::~AbstractOutput()
{
    delete m_waylandOutputDevice.data();
    delete m_xdgOutput.data();
    delete m_waylandOutput.data();
}

QString AbstractOutput::name() const
{
    if (!m_waylandOutput) {
        return i18n("unknown");
    }
    return QStringLiteral("%1 %2").arg(m_waylandOutput->manufacturer()).arg(m_waylandOutput->model());
}

QRect AbstractOutput::geometry() const
{
    return QRect(m_globalPos, pixelSize() / scale());
}

QSize AbstractOutput::physicalSize() const
{
    if (m_orientation == Qt::PortraitOrientation || m_orientation == Qt::InvertedPortraitOrientation) {
        return m_physicalSize.transposed();
    }
    return m_physicalSize;
}

void AbstractOutput::setGlobalPos(const QPoint &pos)
{
    m_globalPos = pos;
    if (m_waylandOutput) {
        m_waylandOutput->setGlobalPosition(pos);
    }
    if (m_waylandOutputDevice) {
        m_waylandOutputDevice->setGlobalPosition(pos);
    }
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalPosition(pos);
        m_xdgOutput->done();
    }
}

void AbstractOutput::setScale(qreal scale)
{
    m_scale = scale;
    if (m_waylandOutput) {
        // this is the scale that clients will ideally use for their buffers
        // this has to be an int which is fine

        // I don't know whether we want to round or ceil
        // or maybe even set this to 3 when we're scaling to 1.5
        // don't treat this like it's chosen deliberately
        m_waylandOutput->setScale(std::ceil(scale));
    }
    if (m_waylandOutputDevice) {
        m_waylandOutputDevice->setScaleF(scale);
    }
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalSize(pixelSize() / m_scale);
        m_xdgOutput->done();
    }
    emit modeChanged();
}

void AbstractOutput::setChanges(KWayland::Server::OutputChangeSet *changes)
{
    qCDebug(KWIN_CORE) << "Set changes in AbstractOutput.";
    Q_ASSERT(!m_waylandOutputDevice.isNull());

    if (!changes) {
        qCDebug(KWIN_CORE) << "No changes.";
        // No changes to an output is an entirely valid thing
    }
    //enabledChanged is handled by plugin code
    if (changes->modeChanged()) {
        qCDebug(KWIN_CORE) << "Setting new mode:" << changes->mode();
        m_waylandOutputDevice->setCurrentMode(changes->mode());
        updateMode(changes->mode());
    }
    if (changes->transformChanged()) {
        qCDebug(KWIN_CORE) << "Server setting transform: " << (int)(changes->transform());
        transform(changes->transform());
    }
    if (changes->positionChanged()) {
        qCDebug(KWIN_CORE) << "Server setting position: " << changes->position();
        setGlobalPos(changes->position());
        // may just work already!
    }
    if (changes->scaleChanged()) {
        qCDebug(KWIN_CORE) << "Setting scale:" << changes->scale();
        setScale(changes->scaleF());
    }
}

void AbstractOutput::setWaylandMode(const QSize &size, int refreshRate)
{
    if (m_waylandOutput.isNull()) {
        return;
    }
    m_waylandOutput->setCurrentMode(size, refreshRate);
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalSize(pixelSize() / scale());
        m_xdgOutput->done();
    }
}

void AbstractOutput::createXdgOutput()
{
    if (!m_waylandOutput || m_xdgOutput) {
        return;
    }
    m_xdgOutput = waylandServer()->xdgOutputManager()->createXdgOutput(m_waylandOutput, m_waylandOutput);
}

void AbstractOutput::setWaylandOutputDevice(KWayland::Server::OutputDeviceInterface *set)
{
    m_waylandOutputDevice = set;
}

void AbstractOutput::initWaylandOutput()
{
    Q_ASSERT(m_waylandOutputDevice);

    if (!m_waylandOutput.isNull()) {
        delete m_waylandOutput.data();
        m_waylandOutput.clear();
    }
    m_waylandOutput = waylandServer()->display()->createOutput();
    createXdgOutput();

    m_waylandOutput->setManufacturer(m_waylandOutputDevice->manufacturer());
    m_waylandOutput->setModel(m_waylandOutputDevice->model());
    m_waylandOutput->setPhysicalSize(rawPhysicalSize());

    for(const auto &mode: m_waylandOutputDevice->modes()) {
        KWayland::Server::OutputInterface::ModeFlags flags;
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Current) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Current;
        }
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Preferred;
        }
        m_waylandOutput->addMode(mode.size, flags, mode.refreshRate);
    }
    m_waylandOutput->create();
}

}
