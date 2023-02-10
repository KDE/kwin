/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects_export.h"

#include <QImage>
#include <QMatrix4x4>

#include <variant>

namespace KWin
{

class GLFramebuffer;

class KWINEFFECTS_EXPORT RenderTarget
{
public:
    explicit RenderTarget(GLFramebuffer *fbo);
    explicit RenderTarget(QImage *image);

    using NativeHandle = std::variant<GLFramebuffer *, QImage *>;
    NativeHandle nativeHandle() const;

    QSize size() const;

private:
    NativeHandle m_nativeHandle;
};

} // namespace KWin
