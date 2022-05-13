/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once
#include "kwin_export.h"

#include <QScopedPointer>
#include <QSharedPointer>

namespace KWin
{
class GLFramebuffer;
class GLTexture;

struct DmaBufAttributes
{
    int planeCount;
    int width;
    int height;
    int format;

    int fd[4];
    int offset[4];
    int pitch[4];
    uint64_t modifier[4];
};

class KWIN_EXPORT DmaBufTexture
{
public:
    explicit DmaBufTexture(QSharedPointer<GLTexture> texture, const DmaBufAttributes &attributes);
    virtual ~DmaBufTexture();

    DmaBufAttributes attributes() const;
    GLTexture *texture() const;
    GLFramebuffer *framebuffer() const;

protected:
    QSharedPointer<GLTexture> m_texture;
    QScopedPointer<GLFramebuffer> m_framebuffer;
    DmaBufAttributes m_attributes;
};

}
