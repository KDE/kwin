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
    explicit RenderTarget(GLFramebuffer *fbo, const QRectF &renderRect, double scale);
    explicit RenderTarget(QImage *image, const QRectF &renderRect, double scale);

    using NativeHandle = std::variant<GLFramebuffer *, QImage *>;
    NativeHandle nativeHandle() const;

    QMatrix4x4 projectionMatrix() const;
    QRectF renderRect() const;
    double scale() const;
    QSize size() const;

    QRectF mapToRenderTarget(const QRectF &logicalGeometry) const;
    QRect mapToRenderTarget(const QRect &logicalGeometry) const;
    QRegion mapToRenderTarget(const QRegion &logicalGeometry) const;

private:
    NativeHandle m_nativeHandle;
    const QRectF m_renderRect;
    const QMatrix4x4 m_projectionMatrix;
    const QMatrix4x4 m_logicalToLocal;
    const double m_scale;
};

} // namespace KWin
