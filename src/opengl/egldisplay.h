/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorspace.h"
#include "core/drm_formats.h"
#include "kwin_export.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSize>
#include <epoxy/egl.h>
#include <map>
#include <sys/types.h>

namespace KWin
{

struct DmaBufAttributes;
class GLTexture;
class GraphicsBuffer;
class DrmDevice;

class KWIN_EXPORT EglDisplay : public QObject
{
    Q_OBJECT

public:
    explicit EglDisplay(::EGLDisplay display, const QList<QByteArray> &extensions, DrmDevice *drmDevice);
    ~EglDisplay() override;

    QList<QByteArray> extensions() const;
    ::EGLDisplay handle() const;
    bool hasExtension(const QByteArray &name) const;

    QString renderNode() const;
    std::optional<dev_t> renderDevNode() const;

    bool isSoftwareRenderer() const;
    bool supportsBufferAge() const;
    bool supportsNativeFence() const;

    const FormatModifierMap &nonExternalOnlySupportedDrmFormats() const;
    const FormatModifierMap &allSupportedDrmFormats() const;
    bool isExternalOnly(uint32_t format, uint64_t modifier) const;

    EGLImageKHR createImage(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) const;
    void destroyImage(EGLImageKHR image) const;

    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &dmabuf,
                                    YUVMatrixCoefficients coefficients = YUVMatrixCoefficients::Identity,
                                    EncodingRange range = EncodingRange::Full) const;

    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer,
                                    YUVMatrixCoefficients coefficients = YUVMatrixCoefficients::Identity,
                                    EncodingRange range = EncodingRange::Full);

    enum class GpuType {
        Internal,
        Discrete,
        Software,
    };
    std::optional<GpuType> type() const;

    static std::unique_ptr<EglDisplay> create(::EGLDisplay display, DrmDevice *drmDevice);

private:
    struct Formats
    {
        FormatModifierMap nonExternalOnly;
        FormatModifierMap all;
    };
    Formats queryImportFormats() const;
    QString determineRenderNode() const;

    const ::EGLDisplay m_handle;
    const QList<QByteArray> m_clientExtensions;
    const QList<QByteArray> m_extensions;
    const QString m_renderNode;
    const std::optional<dev_t> m_renderDevNode;
    DrmDevice *const m_drmDevice;

    const std::optional<GpuType> m_type;
    const bool m_supportsBufferAge;
    const bool m_supportsNativeFence;
    const bool m_isSoftwareRenderer;
    FormatModifierMap m_nonExternalOnlyFormats;
    FormatModifierMap m_allFormats;

    struct
    {
        PFNEGLCREATEIMAGEKHRPROC createImageKHR = nullptr;
        PFNEGLDESTROYIMAGEKHRPROC destroyImageKHR = nullptr;

        PFNEGLQUERYDMABUFFORMATSEXTPROC queryDmaBufFormatsEXT = nullptr;
        PFNEGLQUERYDMABUFMODIFIERSEXTPROC queryDmaBufModifiersEXT = nullptr;
    } m_functions;

    std::map<std::tuple<GraphicsBuffer *, YUVMatrixCoefficients, EncodingRange>, EGLImageKHR> m_importCache;
};

}
