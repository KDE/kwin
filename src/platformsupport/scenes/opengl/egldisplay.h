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
#include <epoxy/egl.h>

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

    bool supportsBufferAge() const;
    bool supportsSwapBuffersWithDamage() const;
    bool supportsNativeFence() const;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const;

    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf) const;

    static std::unique_ptr<EglDisplay> create(::EGLDisplay display, bool owning = true);

private:
    QHash<uint32_t, QList<uint64_t>> queryImportFormats() const;

    const ::EGLDisplay m_handle;
    const QList<QByteArray> m_extensions;
    const bool m_owning;

    const bool m_supportsBufferAge;
    const bool m_supportsSwapBuffersWithDamage;
    const bool m_supportsNativeFence;
    const QHash<uint32_t, QList<uint64_t>> m_importFormats;
};

}
