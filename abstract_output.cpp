/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

void AbstractOutput::applyChanges(const KWaylandServer::OutputChangeSet *changeSet)
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
