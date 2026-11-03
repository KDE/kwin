/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastsource.h"
#include "compositor.h"
#include "core/renderdevice.h"

namespace KWin
{

ScreenCastSource::ScreenCastSource()
    : m_renderDevice(Compositor::self()->primaryDevice())
{
}

ScreenCastSource::~ScreenCastSource()
{
}

bool ScreenCastSource::followsStreamSize()
{
    return false;
}

void ScreenCastSource::resize(const QSize &)
{
    Q_ASSERT(false);
}

RenderDevice *ScreenCastSource::renderDevice() const
{
    return m_renderDevice;
}

} // namespace KWin

#include "moc_screencastsource.cpp"
