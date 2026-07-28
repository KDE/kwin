/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastsource.h"
#include "compositor.h"
#include "core/renderdevice.h"
#include "opengl/eglcontext.h"

namespace KWin
{

ScreenCastSource::ScreenCastSource()
    : m_renderDevice(Compositor::self()->primaryDevice())
    , m_eglContext(m_renderDevice->eglContext())
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

const std::shared_ptr<EglContext> &ScreenCastSource::eglContext() const
{
    return m_eglContext;
}

} // namespace KWin

#include "moc_screencastsource.cpp"
