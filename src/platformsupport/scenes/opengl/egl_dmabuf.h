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
                    DmaBufAttributes &&attrs,
                    quint32 flags,
                    EglDmabuf *interfaceImpl);

    EglDmabufBuffer(const QVector<EGLImage> &images,
                    DmaBufAttributes &&attrs,
                    quint32 flags,
                    EglDmabuf *interfaceImpl);

    ~EglDmabufBuffer() override;

    void setInterfaceImplementation(EglDmabuf *interfaceImpl);
    void setImages(const QVector<EGLImage> &images);
    void removeImages();

    QVector<EGLImage> images() const
    {
        return m_images;
    }

private:
    QVector<EGLImage> m_images;
    EglDmabuf *m_interfaceImpl;
    ImportType m_importType;
};

class KWIN_EXPORT EglDmabuf : public LinuxDmaBufV1RendererInterface
{
public:
    static EglDmabuf *factory(AbstractEglBackend *backend);

    explicit EglDmabuf(AbstractEglBackend *backend);
    ~EglDmabuf() override;

    KWaylandServer::LinuxDmaBufV1ClientBuffer *importBuffer(DmaBufAttributes &&attrs, quint32 flags) override;

    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> tranches() const;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const;

private:
    KWaylandServer::LinuxDmaBufV1ClientBuffer *yuvImport(DmaBufAttributes &&attrs, quint32 flags);

    void setSupportedFormatsAndModifiers();

    AbstractEglBackend *m_backend;
    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<uint32_t, QVector<uint64_t>> m_supportedFormats;

    friend class EglDmabufBuffer;
};

}
