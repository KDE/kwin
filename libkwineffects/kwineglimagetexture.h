/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglutils_export.h>
#include <kwingltexture.h>

typedef void *EGLImageKHR;
typedef void *EGLDisplay;
typedef void *EGLClientBuffer;

namespace KWin
{

class KWINGLUTILS_EXPORT EGLImageTexture : public GLTexture
{
public:
    EGLImageTexture(EGLDisplay display, EGLImageKHR image, int internalFormat, const QSize &size);
    ~EGLImageTexture() override;

private:
    EGLImageKHR m_image;
    EGLDisplay m_display;
};

}

