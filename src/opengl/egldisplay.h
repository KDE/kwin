/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSize>
#include <epoxy/egl.h>

namespace KWin
{

struct DmaBufAttributes;
class GLTexture;

class KWIN_EXPORT EglDisplay
{
public:
    struct DrmFormatInfo
    {
        QList<uint64_t> allModifiers;
        QList<uint64_t> nonExternalOnlyModifiers;
        QList<uint64_t> externalOnlyModifiers;
    };

    EglDisplay(::EGLDisplay display, const QList<QByteArray> &extensions, bool owning = true);
    ~EglDisplay();

    QList<QByteArray> extensions() const;
    ::EGLDisplay handle() const;
    bool hasExtension(const QByteArray &name) const;

    QString renderNode() const;

    bool supportsBufferAge() const;
    bool supportsNativeFence() const;

    QHash<uint32_t, QList<uint64_t>> nonExternalOnlySupportedDrmFormats() const;
    QHash<uint32_t, DrmFormatInfo> allSupportedDrmFormats() const;
    bool isExternalOnly(uint32_t format, uint64_t modifier) const;

    EGLImageKHR createImage(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) const;
    void destroyImage(EGLImageKHR image) const;

    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf, int plane, int format, const QSize &size) const;

    static std::unique_ptr<EglDisplay> create(::EGLDisplay display, bool owning = true);

private:
    QHash<uint32_t, DrmFormatInfo> queryImportFormats() const;

    const ::EGLDisplay m_handle;
    const QList<QByteArray> m_extensions;
    const bool m_owning;

    const bool m_supportsBufferAge;
    const bool m_supportsNativeFence;
    QHash<uint32_t, DrmFormatInfo> m_importFormats;

    struct
    {
        PFNEGLCREATEIMAGEKHRPROC createImageKHR = nullptr;
        PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR = nullptr;

        PFNEGLQUERYDMABUFFORMATSEXTPROC queryDmaBufFormatsEXT = nullptr;
        PFNEGLQUERYDMABUFMODIFIERSEXTPROC queryDmaBufModifiersEXT = nullptr;
    } m_functions;
};

}
