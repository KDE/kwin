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
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/outputchangeset.h>
#include <KWayland/Server/outputdevice_interface.h>
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
}

void AbstractOutput::setChanges(KWayland::Server::OutputChangeSet *changes)
{
    m_changeset = changes;
    qCDebug(KWIN_CORE) << "set changes in AbstractOutput";
    commitChanges();
}

void AbstractOutput::setWaylandOutput(KWayland::Server::OutputInterface *set)
{
    m_waylandOutput = set;
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

}
