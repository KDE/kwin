/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/ninepatch.h"

#include <memory>

class QImage;

namespace KWin
{

class GLTexture;
class EglContext;

class NinePatchOpenGL : public NinePatch
{
public:
    static std::unique_ptr<NinePatchOpenGL> create(const std::shared_ptr<EglContext> &context,
                                                   const QImage &image);
    static std::unique_ptr<NinePatchOpenGL> create(const std::shared_ptr<EglContext> &context,
                                                   const QImage &topLeftPatch,
                                                   const QImage &topPatch,
                                                   const QImage &topRightPatch,
                                                   const QImage &rightPatch,
                                                   const QImage &bottomRightPatch,
                                                   const QImage &bottomPatch,
                                                   const QImage &bottomLeftPatch,
                                                   const QImage &leftPatch);

    explicit NinePatchOpenGL(const std::shared_ptr<EglContext> &eglContext, std::unique_ptr<GLTexture> &&texture);
    ~NinePatchOpenGL() override;

    GLTexture *texture() const;

private:
    std::shared_ptr<EglContext> m_eglContext;
    std::unique_ptr<GLTexture> m_texture;
};

} // namespace KWin
