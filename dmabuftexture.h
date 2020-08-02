/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once
#include "kwin_export.h"
#include <QScopedPointer>

namespace KWin
{
class GLRenderTarget;
class GLTexture;

class KWIN_EXPORT DmaBufTexture
{
public:
    explicit DmaBufTexture(KWin::GLTexture* texture);
    virtual ~DmaBufTexture();

    virtual quint32 stride() const = 0;
    virtual int fd() const = 0;
    KWin::GLRenderTarget* framebuffer() const;

protected:
    QScopedPointer<KWin::GLTexture> m_texture;
    QScopedPointer<KWin::GLRenderTarget> m_framebuffer;
};

}
