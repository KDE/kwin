/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/drm_formats.h"
#include "kwin_export.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSize>
#include <epoxy/egl.h>
#include <sys/types.h>

namespace KWin
{

struct DmaBufAttributes;
class GLTexture;

class KWIN_EXPORT EglDisplay
{
public:
    EglDisplay(::EGLDisplay display, const QList<QByteArray> &extensions, bool owning = true);
    ~EglDisplay();

    QList<QByteArray> extensions() const;
    ::EGLDisplay handle() const;
    bool hasExtension(const QByteArray &name) const;

    QString renderNode() const;
    std::optional<dev_t> renderDevNode() const;

    bool supportsBufferAge() const;
    bool supportsNativeFence() const;

    const FormatModifierMap &nonExternalOnlySupportedDrmFormats() const;
    const FormatModifierMap &allSupportedDrmFormats() const;
    bool isExternalOnly(uint32_t format, uint64_t modifier) const;

    EGLImageKHR createImage(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) const;
    void destroyImage(EGLImageKHR image) const;

    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf, int plane, int format, const QSize &size) const;

    static bool shouldUseOpenGLES();
    static std::unique_ptr<EglDisplay> create(::EGLDisplay display, bool owning = true);

private:
    struct Formats
    {
        FormatModifierMap nonExternalOnly;
        FormatModifierMap all;
    };
    Formats queryImportFormats() const;
    QString determineRenderNode() const;

    const ::EGLDisplay m_handle;
    const QList<QByteArray> m_extensions;
    const bool m_owning;
    const QString m_renderNode;
    const std::optional<dev_t> m_renderDevNode;

    const bool m_supportsBufferAge;
    const bool m_supportsNativeFence;
    FormatModifierMap m_nonExternalOnlyFormats;
    FormatModifierMap m_allFormats;

    struct
    {
        PFNEGLCREATEIMAGEKHRPROC createImageKHR = nullptr;
        PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR = nullptr;

        PFNEGLQUERYDMABUFFORMATSEXTPROC queryDmaBufFormatsEXT = nullptr;
        PFNEGLQUERYDMABUFMODIFIERSEXTPROC queryDmaBufModifiersEXT = nullptr;
    } m_functions;
};

}
