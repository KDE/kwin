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

namespace KWin
{

GammaRamp::GammaRamp(uint32_t size)
    : m_table(3 * size)
    , m_size(size)
{
}

uint32_t GammaRamp::size() const
{
    return m_size;
}

uint16_t *GammaRamp::red()
{
    return m_table.data();
}

const uint16_t *GammaRamp::red() const
{
    return m_table.data();
}

uint16_t *GammaRamp::green()
{
    return m_table.data() + m_size;
}

const uint16_t *GammaRamp::green() const
{
    return m_table.data() + m_size;
}

uint16_t *GammaRamp::blue()
{
    return m_table.data() + 2 * m_size;
}

const uint16_t *GammaRamp::blue() const
{
    return m_table.data() + 2 * m_size;
}

AbstractOutput::AbstractOutput(QObject *parent)
    : QObject(parent)
{
}

AbstractOutput::~AbstractOutput()
{
}

QByteArray AbstractOutput::uuid() const
{
    return QByteArray();
}

void AbstractOutput::setEnabled(bool enable)
{
    Q_UNUSED(enable)
}

void AbstractOutput::applyChanges(const KWayland::Server::OutputChangeSet *changeSet)
{
    Q_UNUSED(changeSet)
}

bool AbstractOutput::isInternal() const
{
    return false;
}

qreal AbstractOutput::scale() const
{
    return 1;
}

QSize AbstractOutput::physicalSize() const
{
    return QSize();
}

Qt::ScreenOrientation AbstractOutput::orientation() const
{
    return Qt::PrimaryOrientation;
}

int AbstractOutput::gammaRampSize() const
{
    return 0;
}

bool AbstractOutput::setGammaRamp(const GammaRamp &gamma)
{
    Q_UNUSED(gamma);
    return false;
}

} // namespace KWin
