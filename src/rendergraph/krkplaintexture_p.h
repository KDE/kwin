/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "krknativetexture.h"
#include "krktexture.h"

#include <QMatrix4x4>

namespace KWin
{

class KWIN_EXPORT KrkPlainTextureOpenGL : public KrkTexture
{
    Q_OBJECT

public:
    explicit KrkPlainTextureOpenGL(GLTexture *texture);
    ~KrkPlainTextureOpenGL() override;

    bool ownsTexture() const;
    void setOwnsTexture(bool owns);

    bool hasAlphaChannel() const override;
    void setHasAlphaChannel(bool has);
    KrkNative::KrkNativeTexture *nativeTexture() const override;

private:
    KrkNative::KrkOpenGLTexture m_nativeTexture;
    bool m_hasAlphaChannel = false;
    bool m_ownsTexture = false;
};

class KWIN_EXPORT KrkPlainTextureSoftware : public KrkTexture
{
    Q_OBJECT

public:
    explicit KrkPlainTextureSoftware(const QImage &image);

    bool hasAlphaChannel() const override;
    KrkNative::KrkNativeTexture *nativeTexture() const override;

private:
    KrkNative::KrkSoftwareTexture m_nativeTexture;
};

} // namespace KWin
