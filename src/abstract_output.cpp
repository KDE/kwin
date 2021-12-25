/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "abstract_output.h"
#include <KSharedConfig>
#include <KConfigGroup>

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

QDebug operator<<(QDebug debug, const AbstractOutput *output)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    if (output) {
        debug << output->metaObject()->className() << '(' << static_cast<const void *>(output);
        debug << ", name=" << output->name();
        debug << ", geometry=" << output->geometry();
        debug << ", scale=" << output->scale();
        if (debug.verbosity() > 2) {
            debug << ", manufacturer=" << output->manufacturer();
            debug << ", model=" << output->model();
            debug << ", serialNumber=" << output->serialNumber();
        }
        debug << ')';
    } else {
        debug << "AbstractOutput(0x0)";
    }
    return debug;
}

AbstractOutput::AbstractOutput(QObject *parent)
    : QObject(parent)
{
}

AbstractOutput::~AbstractOutput()
{
}

QUuid AbstractOutput::uuid() const
{
    return QUuid();
}

bool AbstractOutput::isEnabled() const
{
    return true;
}

void AbstractOutput::setEnabled(bool enable)
{
    Q_UNUSED(enable)
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

QString AbstractOutput::manufacturer() const
{
    return QString();
}

QString AbstractOutput::model() const
{
    return QString();
}

QString AbstractOutput::serialNumber() const
{
    return QString();
}

RenderLoop *AbstractOutput::renderLoop() const
{
    return nullptr;
}

void AbstractOutput::inhibitDirectScanout()
{
    m_directScanoutCount++;
}
void AbstractOutput::uninhibitDirectScanout()
{
    m_directScanoutCount--;
}

bool AbstractOutput::directScanoutInhibited() const
{
    return m_directScanoutCount;
}

std::chrono::milliseconds AbstractOutput::dimAnimationTime()
{
    // See kscreen.kcfg
    return std::chrono::milliseconds (KSharedConfig::openConfig()->group("Effect-Kscreen").readEntry("Duration", 250));
}

bool AbstractOutput::usesSoftwareCursor() const
{
    return true;
}

} // namespace KWin
