/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "../../../linux_dmabuf.h"

#include "abstract_egl_backend.h"

#include <QVector>

namespace KWin
{
class EglDmabuf;

class EglDmabufBuffer : public DmabufBuffer
{
public:
    using Plane = KWaylandServer::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWaylandServer::LinuxDmabufUnstableV1Interface::Flags;

    enum class ImportType {
        Direct,
        Conversion
    };

    EglDmabufBuffer(EGLImage image,
                    const QVector<Plane> &planes,
                    uint32_t format,
                    const QSize &size,
                    Flags flags,
                    EglDmabuf *interfaceImpl);

    EglDmabufBuffer(const QVector<Plane> &planes,
                    uint32_t format,
                    const QSize &size,
                    Flags flags,
                    EglDmabuf *interfaceImpl);

    ~EglDmabufBuffer() override;

    void setInterfaceImplementation(EglDmabuf *interfaceImpl);
    void addImage(EGLImage image);
    void removeImages();

    QVector<EGLImage> images() const { return m_images; }

private:
    QVector<EGLImage> m_images;
    EglDmabuf *m_interfaceImpl;
    ImportType m_importType;
};

class EglDmabuf : public LinuxDmabuf
{
public:
    using Plane = KWaylandServer::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWaylandServer::LinuxDmabufUnstableV1Interface::Flags;

    static EglDmabuf* factory(AbstractEglBackend *backend);

    explicit EglDmabuf(AbstractEglBackend *backend);
    ~EglDmabuf() override;

    KWaylandServer::LinuxDmabufUnstableV1Buffer *importBuffer(const QVector<Plane> &planes,
                                                                uint32_t format,
                                                                const QSize &size,
                                                                Flags flags) override;

private:
    EGLImage createImage(const QVector<Plane> &planes,
                         uint32_t format,
                         const QSize &size);

    KWaylandServer::LinuxDmabufUnstableV1Buffer *yuvImport(const QVector<Plane> &planes,
                                                             uint32_t format,
                                                             const QSize &size,
                                                             Flags flags);

    void setSupportedFormatsAndModifiers();

    AbstractEglBackend *m_backend;

    friend class EglDmabufBuffer;
};

}
