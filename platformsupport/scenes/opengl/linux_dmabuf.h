/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2019 Roman Gilg <subdiff@gmail.com>
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>

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
#pragma once

#include "abstract_egl_backend.h"

#include <KWayland/Server/linuxdmabuf_v1_interface.h>

#include <QVector>

namespace KWin
{
class LinuxDmabuf;

class DmabufBuffer : public KWayland::Server::LinuxDmabufUnstableV1Buffer
{
public:
    using Plane = KWayland::Server::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWayland::Server::LinuxDmabufUnstableV1Interface::Flags;

    enum class ImportType {
        Direct,
        Conversion
    };

    DmabufBuffer(EGLImage image,
                 const QVector<Plane> &planes,
                 uint32_t format,
                 const QSize &size,
                 Flags flags,
                 LinuxDmabuf *interfaceImpl);

    DmabufBuffer(const QVector<Plane> &planes,
                 uint32_t format,
                 const QSize &size,
                 Flags flags,
                 LinuxDmabuf *interfaceImpl);

    ~DmabufBuffer() override;

    void addImage(EGLImage image);
    void removeImages();

    QVector<EGLImage> images() const { return m_images; }
    Flags flags() const { return m_flags; }
    const QVector<Plane> &planes() const { return m_planes; }

private:
    QVector<EGLImage> m_images;
    QVector<Plane> m_planes;
    Flags m_flags;
    LinuxDmabuf *m_interfaceImpl;
    ImportType m_importType;
};

class LinuxDmabuf : public KWayland::Server::LinuxDmabufUnstableV1Interface::Impl
{
public:
    using Plane = KWayland::Server::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWayland::Server::LinuxDmabufUnstableV1Interface::Flags;

    static LinuxDmabuf* factory(AbstractEglBackend *backend);

    explicit LinuxDmabuf(AbstractEglBackend *backend);
    ~LinuxDmabuf();

    KWayland::Server::LinuxDmabufUnstableV1Buffer *importBuffer(const QVector<Plane> &planes,
                                                                uint32_t format,
                                                                const QSize &size,
                                                                Flags flags) override;

private:
    EGLImage createImage(const QVector<Plane> &planes,
                         uint32_t format,
                         const QSize &size);


    KWayland::Server::LinuxDmabufUnstableV1Buffer *yuvImport(const QVector<Plane> &planes,
                                                             uint32_t format,
                                                             const QSize &size,
                                                             Flags flags);

    void removeBuffer(DmabufBuffer *buffer);
    void setSupportedFormatsAndModifiers();

    KWayland::Server::LinuxDmabufUnstableV1Interface *m_interface;
    QSet<DmabufBuffer*> m_buffers;
    AbstractEglBackend *m_backend;

    friend class DmabufBuffer;
};

}
