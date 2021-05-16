/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clientbuffer.h"

#include <epoxy/gl.h>

#include <QImage>
#include <QOpenGLFramebufferObject>

namespace KWin
{
class ClientBufferRef;

class KWIN_EXPORT ClientBufferInternal : public ClientBuffer
{
    Q_OBJECT

public:
    ClientBufferInternal(const QImage &image, qreal scale);
    ClientBufferInternal(QOpenGLFramebufferObject *fbo, qreal scale);

    bool hasAlphaChannel() const override;
    QSize size() const override;

    QImage image() const;
    QOpenGLFramebufferObject *fbo() const;
    qreal devicePixelRatio() const;

    static ClientBufferInternal *from(const ClientBufferRef &ref);

protected:
    void release() override;

private:
    QImage m_image;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;
    qreal m_devicePixelRatio;
};

} // namespace KWin
