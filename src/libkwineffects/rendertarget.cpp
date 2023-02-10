/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rendertarget.h"
#include "kwinglutils.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo)
    : m_nativeHandle(fbo)
{
}

RenderTarget::RenderTarget(QImage *image)
    : m_nativeHandle(image)
{
}

QSize RenderTarget::size() const
{
    if (auto fbo = std::get_if<GLFramebuffer *>(&m_nativeHandle)) {
        return (*fbo)->size();
    } else if (auto image = std::get_if<QImage *>(&m_nativeHandle)) {
        return (*image)->size();
    } else {
        Q_UNREACHABLE();
    }
}

RenderTarget::NativeHandle RenderTarget::nativeHandle() const
{
    return m_nativeHandle;
}

} // namespace KWin
