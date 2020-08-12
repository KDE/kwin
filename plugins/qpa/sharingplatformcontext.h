/*
    KWin - the KDE window manager

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_SHARINGPLATFORMCONTEXT_H
#define KWIN_QPA_SHARINGPLATFORMCONTEXT_H

#include "abstractplatformcontext.h"

namespace KWin
{
namespace QPA
{

class SharingPlatformContext : public AbstractPlatformContext
{
public:
    explicit SharingPlatformContext(QOpenGLContext *context);
    SharingPlatformContext(QOpenGLContext *context, const EGLSurface &surface, EGLConfig config = nullptr);

    void swapBuffers(QPlatformSurface *surface) override;

    GLuint defaultFramebufferObject(QPlatformSurface *surface) const override;

    bool makeCurrent(QPlatformSurface *surface) override;

    bool isSharing() const override;

private:
    void create();

    EGLSurface m_surface;
};

}
}

#endif
