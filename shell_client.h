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
#ifndef KWIN_SHELL_CLIENT_H
#define KWIN_SHELL_CLIENT_H

#include "toplevel.h"

namespace KWayland
{
namespace Server
{
class ShellSurfaceInterface;
}
}

namespace KWin
{

class ShellClient : public Toplevel
{
    Q_OBJECT
public:
    ShellClient(KWayland::Server::ShellSurfaceInterface *surface);
    virtual ~ShellClient();

    QStringList activities() const override;
    QPoint clientPos() const override;
    QSize clientSize() const override;
    int desktop() const override;
    Layer layer() const override;
    QRect transparentRect() const override;
    bool shouldUnredirect() const override;
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    void debug(QDebug &stream) const override;
    double opacity() const override;
    QByteArray windowRole() const override;

    KWayland::Server::ShellSurfaceInterface *shellSurface() const {
        return m_shellSurface;
    }

protected:
    void addDamage(const QRegion &damage) override;

private:
    void setGeometry(const QRect &rect);
    void destroyClient();
    static void deleteClient(ShellClient *c);

    KWayland::Server::ShellSurfaceInterface *m_shellSurface;
    QSize m_clientSize;
};

}

#endif
