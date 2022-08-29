/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QImage>

#include <variant>

namespace KWin
{

class GLFramebuffer;

class KWIN_EXPORT RenderTarget
{
public:
    RenderTarget();
    explicit RenderTarget(GLFramebuffer *fbo);
    explicit RenderTarget(QImage *image);

    QSize size() const;

    using NativeHandle = std::variant<GLFramebuffer *, QImage *>;
    NativeHandle nativeHandle() const;

    void setDevicePixelRatio(qreal ratio);
    qreal devicePixelRatio() const;

private:
    NativeHandle m_nativeHandle;
    qreal m_devicePixelRatio = 1;
};

} // namespace KWin
