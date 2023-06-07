/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwingltexture.h"
#include "libkwineffects/kwinglutils_export.h"

typedef void *EGLImageKHR;
typedef void *EGLDisplay;
typedef void *EGLClientBuffer;

namespace KWin
{

class KWINGLUTILS_EXPORT EGLImageTexture : public GLTexture
{
public:
    explicit EGLImageTexture(::EGLDisplay display, EGLImageKHR image, uint textureId, int internalFormat, const QSize &size, uint32_t target);
    ~EGLImageTexture() override;

    static std::shared_ptr<EGLImageTexture> create(::EGLDisplay display, EGLImageKHR image, int internalFormat, const QSize &size, bool externalOnly);

    EGLImageKHR m_image;
    ::EGLDisplay m_display;
};

}
