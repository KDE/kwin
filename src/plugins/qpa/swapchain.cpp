/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "swapchain.h"
#include "clientbuffer_internal.h"

namespace KWin
{
namespace QPA
{

Swapchain::Swapchain(const QSize &size, qreal devicePixelRatio)
    : m_size(size)
    , m_devicePixelRatio(devicePixelRatio)
{
    for (int i = 0; i < 2; ++i) {
        m_buffers.append(allocate());
    }
}

Swapchain::~Swapchain()
{
    for (ClientBufferInternal *buffer : qAsConst(m_buffers)) {
        buffer->markAsDestroyed();
    }
}

QSize Swapchain::size() const
{
    return m_size;
}

qreal Swapchain::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

ClientBufferInternal *Swapchain::acquire()
{
    ClientBufferInternal *ret = nullptr;
    for (ClientBufferInternal *buffer : qAsConst(m_buffers)) {
        if (!buffer->isReferenced()) {
            ret = buffer;
            break;
        }
    }

    if (!ret) {
        ret = allocate();
        m_buffers.append(ret);
    }

    return ret;
}

ClientBufferInternal *Swapchain::allocate()
{
    return new ClientBufferInternal(new QOpenGLFramebufferObject(size(), QOpenGLFramebufferObject::CombinedDepthStencil), devicePixelRatio());
}

} // namespace QPA
} // namespace KWin
