/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "abstract_egl_backend.h"

#include "linux_dmabuf.h"

#include <QVector>

namespace KWin
{
class EglDmabuf;

class EglDmabufBuffer : public LinuxDmaBufV1ClientBuffer
{
public:
    enum class ImportType {
        Direct,
        Conversion
    };

    EglDmabufBuffer(EGLImage image,
                    const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                    quint32 format,
                    const QSize &size,
                    quint32 flags,
                    EglDmabuf *interfaceImpl);

    EglDmabufBuffer(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                    quint32 format,
                    const QSize &size,
                    quint32 flags,
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

class EglDmabuf : public LinuxDmaBufV1RendererInterface
{
public:
    static EglDmabuf* factory(AbstractEglBackend *backend);

    explicit EglDmabuf(AbstractEglBackend *backend);
    ~EglDmabuf() override;

    KWaylandServer::LinuxDmaBufV1ClientBuffer *importBuffer(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                                            quint32 format,
                                                            const QSize &size,
                                                            quint32 flags) override;

    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> tranches() const {
        return m_tranches;
    }

private:
    EGLImage createImage(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                         uint32_t format,
                         const QSize &size);

    KWaylandServer::LinuxDmaBufV1ClientBuffer *yuvImport(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                                         quint32 format,
                                                         const QSize &size,
                                                         quint32 flags);

    void setSupportedFormatsAndModifiers();

    AbstractEglBackend *m_backend;
    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> m_tranches;

    friend class EglDmabufBuffer;
};

}
