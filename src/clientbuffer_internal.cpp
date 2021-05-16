/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientbuffer_internal.h"
#include "clientbufferref.h"

namespace KWin
{

ClientBufferInternal::ClientBufferInternal(const QImage &image, qreal scale)
    : m_image(image)
    , m_devicePixelRatio(scale)
{
}

ClientBufferInternal::ClientBufferInternal(QOpenGLFramebufferObject *fbo, qreal scale)
    : m_fbo(fbo)
    , m_devicePixelRatio(scale)
{
}

bool ClientBufferInternal::hasAlphaChannel() const
{
    return true;
}

QSize ClientBufferInternal::size() const
{
    if (m_fbo) {
        return m_fbo->size();
    } else if (!m_image.isNull()) {
        return m_image.size();
    } else {
        Q_UNREACHABLE();
    }
}

void ClientBufferInternal::release()
{
}

QImage ClientBufferInternal::image() const
{
    return m_image;
}

QOpenGLFramebufferObject *ClientBufferInternal::fbo() const
{
    return m_fbo.data();
}

qreal ClientBufferInternal::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

ClientBufferInternal *ClientBufferInternal::from(const ClientBufferRef &ref)
{
    return qobject_cast<ClientBufferInternal *>(ref.internalBuffer());
}

} // namespace KWin
