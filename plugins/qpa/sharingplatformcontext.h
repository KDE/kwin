/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_QPA_SHARINGPLATFORMCONTEXT_H
#define KWIN_QPA_SHARINGPLATFORMCONTEXT_H

#include "abstractplatformcontext.h"

namespace KWin
{
namespace QPA
{
class Integration;

class SharingPlatformContext : public AbstractPlatformContext
{
public:
    explicit SharingPlatformContext(QOpenGLContext *context, Integration *integration);
    explicit SharingPlatformContext(QOpenGLContext *context, Integration *integration, const EGLSurface &surface, EGLConfig config = nullptr);

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
